#include "relay_packet.h"
#include "relay_client.h"
#include "debug.h"

/* Socket adapter uses fd adapter for IO */
#define RCA_SOCKET_USE_FD

size_t relay_client_mtu = 1L << 31;

static bool relay_authenticate(struct relay_client *self)
{
	if (strlen(self->local) == 0) {
		return true;
	}
	log_debug("Authenticating relay client with name '%s'", self->local);
	if (!relay_client_send_packet(self, "AUTH", "", self->local, -1)) {
		log_error("Failed to send authentication packet");
		return false;
	}
	struct relay_packet *rp = relay_client_recv_packet(self);
	if (!rp) {
		log_error("Failed to receive authentication response packet");
		return false;
	}
	if (strncmp(rp->type, "AUTH", RELAY_ENDPOINT_LENGTH) != 0) {
		free(rp);
		log_error("Invalid authentication response packet (type='%s')", rp->type);
		return false;
	}
	free(rp);
	return true;
}

/* File-descriptor adapter */

struct rca_fd_data {
	int fd;
	bool owns_fd;
};

static bool rca_fd_init_int(struct relay_client *self, struct rca_fd_data *this, const struct relay_client_fd_data *args)
{
	if (this->fd < 0) {
		return false;
	}
	this->fd = args->fd;
	this->owns_fd = args->owns;
	struct stat ss;
	if (fstat(this->fd, &ss) == 0 && S_ISSOCK(ss.st_mode)) {
		log_debug("Configuring socket interface");
		setsockopt_nodelay(this->fd);
		setsockopt_keepalive(this->fd);
	}
	/* Authenticate if fd is backed by a socket */
	if (args->auth_needed && !relay_authenticate(self)) {
		log_error("Failed to authenticate with fd#%d (%d)", this->fd, errno);
		return false;
	}
	return true;
}

static void rca_fd_destroy_int(struct relay_client *self, struct rca_fd_data *this)
{
	(void) self;
	if (this->owns_fd) {
		close(this->fd);
	}
}

static bool again(int res)
{
	return res == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK);
}

static bool poll_one(int fd, int event) {
	struct pollfd pfd = { .fd = fd, .events = event, .revents = 0 };
	do {
		errno = 0;
	} while (poll(&pfd, 1, -1) != 1 || errno == EAGAIN);
	return (pfd.revents & event) && !(pfd.revents & POLLERR);
}

static bool rca_fd_send_int(struct rca_fd_data *this, const void *buf, size_t length)
{
	const char *ptr = buf;
	while (length) {
		errno = 0;
		ssize_t bytes = write(this->fd, ptr, length);
		if (again(bytes)) {
			if (!poll_one(this->fd, POLLOUT)) {
				log_error("poll", "%d, POLLOUT", this->fd);
				return false;
			}
			continue;
		} else if (bytes == -1) {
			log_error("Failed to send %zu bytes on fd (%d)", length, errno);
			return false;
		}
		length -= bytes;
		ptr += bytes;
	}
	return fsync(this->fd) == 0 || errno == EINVAL;
}

static bool rca_fd_recv_int(struct rca_fd_data *this, void *buf, size_t length)
{
#if defined DEBUG_VERBOSE_relay
	int waiting;
	ioctl(this->fd, FIONREAD, &waiting);
	log_debug_v("Reading %zu bytes (%u available)", length, waiting);
#endif
	char *ptr = buf;
	while (length) {
		errno = 0;
		ssize_t bytes = read(this->fd, ptr, length);
		if (again(bytes)) {
			if (!poll_one(this->fd, POLLIN)) {
				log_error("poll", "%d, POLLIN", this->fd);
				return false;
			}
			continue;
		} else if (bytes == -1) {
			log_error("Failed to read %zu bytes on fd (%d)", length, errno);
			return false;
		} else if (bytes == 0) {
			/* EOF */
			/* log_sysfail("read", "%d, ..., %zu", this->fd, length); */
			log_debug("EOF fd=%d read_len=%zu", this->fd, length);
			return false;
		}
		length -= bytes;
		ptr += bytes;
	}
	return true;
}

static bool rca_fd_init(struct relay_client *self, const void *initargs)
{
	return rca_fd_init_int(self, self->data, initargs);
}

static void rca_fd_destroy(struct relay_client *self)
{
	rca_fd_destroy_int(self, self->data);
}

static bool rca_fd_send(struct relay_client *self, const void *buf, size_t length)
{
	return rca_fd_send_int(self->data, buf, length);
}

static bool rca_fd_recv(struct relay_client *self, void *buf, size_t length)
{
	return rca_fd_recv_int(self->data, buf, length);
}

const struct relay_client_adapter relay_client_fd_adapter = {
	.init = rca_fd_init,
	.destroy = rca_fd_destroy,
	.send = rca_fd_send,
	.recv = rca_fd_recv,
	.instdata_size = sizeof(struct rca_fd_data)
};

/* Socket adapter */

struct rca_socket_data {
	struct socket_client socket;
#if defined RCA_SOCKET_USE_FD
	struct rca_fd_data fd;
#endif
};

static bool rca_socket_init(struct relay_client *self, const void *initargs)
{
	struct rca_socket_data *this = self->data;
	const struct relay_client_socket_data *args = initargs;
	/* Connect and configure socket */
	struct fstr addr;
	struct fstr port;
	fstr_init_ref(&addr, args->addr);
	fstr_init_ref(&port, args->port);
	if (!socket_client_init(&this->socket, &addr, &port, NULL)) {
		log_error("Failed to connect to relay socket " PRIfs ":" PRIfs " (%d)", prifs(&addr), prifs(&port), errno);
		return false;
	}
#if defined RCA_SOCKET_USE_FD
	const struct relay_client_fd_data fdargs = {
		.fd = this->socket.fd,
		.owns = false,
		.auth_needed = true
	};
	return rca_fd_init_int(self, &this->fd, &fdargs);
#else
	setsockopt_nodelay(this->socket.fd);
	setsockopt_keepalive(this->socket.fd);
	if (!relay_authenticate(self)) {
		log_error("Failed to authenticate with " PRIfs ":" PRIfs " (%d)", prifs(&addr), prifs(&port), errno);
		return false;
	}
#endif
	return true;
}

static void rca_socket_destroy(struct relay_client *self)
{
	struct rca_socket_data *this = self->data;
#if defined RCA_SOCKET_USE_FD
	rca_fd_destroy_int(self, &this->fd);
#endif
	socket_client_destroy(&this->socket);
}

static bool rca_socket_send(struct relay_client *self, const void *buf, size_t length)
{
	struct rca_socket_data *this = self->data;
#if defined RCA_SOCKET_USE_FD
	return rca_fd_send_int(&this->fd, buf, length);
#else
	return socket_client_send(&this->socket, buf, length);
#endif
}

static bool rca_socket_recv(struct relay_client *self, void *buf, size_t length)
{
	struct rca_socket_data *this = self->data;
#if defined RCA_SOCKET_USE_FD
	return rca_fd_recv_int(&this->fd, buf, length);
#else
	if (!socket_client_peek(&this->socket, buf, length)) {
		log_error("Failed to wait/peek %zu bytes on socket (%d)", length, errno);
		return false;
	}
	if (!socket_client_recv(&this->socket, buf, length)) {
		log_error("Failed to receive %zu bytes on socket (%d)", length, errno);
		return false;
	}
	return true;
#endif
}

const struct relay_client_adapter relay_client_socket_adapter = {
	.init = rca_socket_init,
	.destroy = rca_socket_destroy,
	.send = rca_socket_send,
	.recv = rca_socket_recv,
	.instdata_size = sizeof(struct rca_socket_data)
};

/* I/O */

static bool relay_client_write(struct relay_client *self, const void *buf, const size_t length)
{
	if (self->failed) {
		log_error("Attempted to write to relay client while in failed state");
		return false;
	}
	log_debug("Writing %zu bytes", length);
	bool res =  self->adapter->send(self, buf, length);
	if (res) {
		log_debug("Written %zu bytes", length);
	} else {
		log_error("Relay write failed (errno=%d, bytes=%zu)", errno, length);
	}
	return res;
}

static bool relay_client_read(struct relay_client *self, void *buf, size_t length)
{
	if (self->failed) {
		log_error("Attempted to read from relay client while in failed state");
		return false;
	}
	bool res = self->adapter->recv(self, buf, length);
	if (res) {
		log_debug("Read %zu bytes", length);
	} else {
		log_error("Read failed (errno=%d, bytes=%zu)", errno, length);
	}
	return res;
}

/* Convenience constructors */

bool relay_client_init_socket(struct relay_client *self, const char *local, const char *addr, const char *port)
{
	struct relay_client_socket_data args = {
		.addr = addr,
		.port = port
	};
	return relay_client_init(self, local, &relay_client_socket_adapter, &args);
}

bool relay_client_init_fd(struct relay_client *self, const char *local, int fd, bool owns, bool auth_needed)
{
	struct relay_client_fd_data args = {
		.fd = fd,
		.owns = owns,
		.auth_needed = auth_needed
	};
	return relay_client_init(self, local, &relay_client_fd_adapter, &args);
}

/* Life-cycle */
bool relay_client_init(struct relay_client *self, const char *local, const struct relay_client_adapter *adapter, const void *args)
{
	/* Zero-init */
	memset(self, 0, sizeof(*self));
	/* Copy name in, if one was provided */
	if (local != NULL) {
		int len = strnlen(local, RELAY_ENDPOINT_LENGTH + 1);
		if (len == RELAY_ENDPOINT_LENGTH + 1) {
			log_error("Invalid endpoint name: '%s'", local);
			goto fail;
		}
		memcpy(self->local, local, len);
	}
	/* Other config */
	self->mtu = relay_client_mtu;
	self->adapter = adapter;
	/* Child constructor */
	self->data = malloc(adapter->instdata_size);
	if (!self->data) {
		goto fail;
	}
	memset(self->data, 0, adapter->instdata_size);
	if (!adapter->init(self, args)) {
		log_error("Relay client adapter failed to initiailise");
		goto child_fail;
	}
	return true;
child_fail:
	relay_client_destroy(self);
fail:
	self->failed |= RCF_INIT;
	return false;
}

void relay_client_destroy(struct relay_client *self)
{
	if (self->adapter) {
		self->adapter->destroy(self);
		self->adapter = NULL;
	}
	free(self->data);
	self->data = NULL;
}

/* Writing */

bool relay_client_send_text(struct relay_client *self, const char *type, const char *remote, const char *text)
{
	return relay_client_send_packet(self, type, remote, text, strlen(text));
}

bool relay_client_send_packet(struct relay_client *self, const char *type, const char *remote, const void *data, const size_t length)
{
	struct relay_packet p;
	relay_make_packet(&p, type, remote, self->local, (char *) data, length);
	return relay_client_send_packet2(self, &p);
}

bool relay_client_send_packet2(struct relay_client *self, const struct relay_packet *packet)
{
// fprintf(stderr, "Sending '%s' from '%s' to '%s'\n", packet->type, packet->local, packet->remote);
	size_t total_length = relay_serialised_packet_size(packet->length);
	char buf[total_length];
	struct relay_packet_serial *s = (void *) buf;
	relay_serialise_packet(s, packet, &total_length);
	bool res = relay_client_send_packet3(self, s, total_length);
	return res;
}

bool relay_client_send_packet3(struct relay_client *self, const struct relay_packet_serial *packet, size_t total_length)
{
	if (total_length == 0) {
		total_length = sizeof(*packet) + ntohl(packet->header.length);
	}
	if (total_length > self->mtu) {
		self->failed |= RCF_SEND_TOO_LARGE;
		log_error("Attempted to send packet larger (%zu) than client MTU (%zu)", total_length, self->mtu);
		return false;
	}
	if (!relay_client_write(self, packet, total_length)) {
		log_error("Failed to write %zu bytes (%d)", total_length, errno);
		return false;
	}
	return true;
}

/* Reading */

static ssize_t relay_client_read_hdr(struct relay_client *self)
{
	if (!self->has_header) {
		log_debug("Reading header");
		if (!relay_client_read(self, &self->hdr, sizeof(self->hdr))) {
			log_error("Failed to read relay packet header (%d)", errno);
			return -1;
		}
		self->has_header = true;
	}
	return ntohl(self->hdr.length);
}

static bool relay_client_read_payload(struct relay_client *self, struct relay_packet_serial *ps)
{
	if (!self->has_header) {
		log_error("Attempted to read packet payload before header");
		return false;
	}
	log_debug("Reading payload");
	const size_t length = ntohl(self->hdr.length);
	if (length > 0 && !relay_client_read(self, ps->data, length)) {
		log_error("Failed to read relay packet payload (%d)", errno);
		return false;
	}
	self->has_header = false;
	memcpy(ps, &self->hdr, sizeof(self->hdr));
	return true;
}

struct relay_packet_serial *relay_client_recv_serialised_packet(struct relay_client *self)
{
	ssize_t data_length = relay_client_read_hdr(self);
	if (data_length == -1) {
		log_error("Failed to read raw packet header (%d)", errno);
		return NULL;
	}
	struct relay_packet_serial *ps;
	size_t in_length = sizeof(self->hdr) + data_length;
	/* MTU/max-size check */
	if (in_length > self->mtu) {
		self->failed |= RCF_RECV_TOO_LARGE;
		log_error("Attempted to receive packet larger (%zu) than client MTU (%zu)", in_length, self->mtu);
		return NULL;
	}
	/* Add extra byte for null-terminator */
	size_t alloc_length = sizeof(*ps) + data_length + 1;
	ps = malloc(alloc_length);
	/* Add null-terminator to after received data block */
	ps->data[data_length] = 0;
	if (!relay_client_read_payload(self, ps)) {
		free(ps);
		log_error("Failed to read raw packet payload (%d)", errno);
		return NULL;
	}
	return ps;
}

struct relay_packet *relay_client_recv_packet(struct relay_client *self)
{
	ssize_t data_length = relay_client_read_hdr(self);
	if (data_length == -1) {
		log_error("Failed to read packet header (%d)", errno);
		return NULL;
	}
	/*
	 * Deserialised packet points to buffers in serialised packet, so
	 * allocate them both at once with the deserialised one at the head, so
	 * we can return it along with the dependent data and free them both at
	 * once easily.
	 */
	struct {
		/* Must be first member due to aliasing of return value */
		struct relay_packet p;
		union {
			struct relay_packet_serial ps;
		};
	} *tuple;
	size_t in_length = sizeof(self->hdr) + data_length;
	/* MTU/max-size check */
	if (in_length > self->mtu) {
		self->failed |= RCF_RECV_TOO_LARGE;
		log_error("Attempted to receive packet larger (%zu) than client MTU (%zu)", in_length, self->mtu);
		return NULL;
	}
	/* Add extra byte for null-terminator */
	size_t alloc_length = sizeof(*tuple) + data_length + 1;
	tuple = malloc(alloc_length);
	/* Add null-terminator to after received data block */
	tuple->ps.data[data_length] = 0;
	if (!relay_client_read_payload(self, &tuple->ps)) {
		log_error("Failed to read packet payload (%d)", errno);
		free(tuple);
		return NULL;
	}
	relay_deserialise_packet(&tuple->p, &tuple->ps, in_length);
	return &tuple->p;
}

ssize_t relay_client_recv_data(struct relay_client *self, char *type, char *remote, char *local, char *buf, size_t buf_size)
{
	struct relay_packet_serial *packet = relay_client_recv_serialised_packet(self);
	ssize_t res = relay_explode_serialised_packet(packet, type, remote, local, buf, buf_size);
	free(packet);
	return res;
}
