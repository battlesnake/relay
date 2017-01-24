#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "relay_packet.h"
#include "relay_client.h"
#include "debug.h"

size_t relay_client_mtu = 1L << 31;

/* Socket adapter */

struct rca_socket_data {
	struct socket_client socket;
};

static bool rca_socket_init(struct relay_client *self, const void *initargs)
{
	struct rca_socket_data *this = self->data;
	const struct relay_client_socket_data *args = initargs;
	/* Connect and configure socket */
	if (!socket_client_init(&this->socket, args->addr, args->port, NULL)) {
		return false;
	}
	setsockopt_nodelay(this->socket.fd);
	setsockopt_keepalive(this->socket.fd);
	/* Authenticate if name was provided */
	if (self->local[0] && !relay_client_send_packet(self, "AUTH", "", self->local, -1)) {
		return false;
	}
	return true;
}

static void rca_socket_destroy(struct relay_client *self)
{
	struct rca_socket_data *this = self->data;
	socket_client_destroy(&this->socket);
}

static bool rca_socket_send(struct relay_client *self, const void *buf, size_t length)
{
	struct rca_socket_data *this = self->data;
	return socket_client_send(&this->socket, buf, length);
}

static bool rca_socket_recv(struct relay_client *self, void *buf, size_t length)
{
	struct rca_socket_data *this = self->data;
	return socket_client_peek(&this->socket, buf, length) && socket_client_recv(&this->socket, buf, length);
}

const struct relay_client_adapter relay_client_socket_adapter = {
	.init = rca_socket_init,
	.destroy = rca_socket_destroy,
	.send = rca_socket_send,
	.recv = rca_socket_recv,
	.instdata_size = sizeof(struct rca_socket_data)
};

/* File-descriptor adapter */

struct rca_fd_data {
	int fd;
	bool owns_fd;
};

static bool rca_fd_init(struct relay_client *self, const void *initargs)
{
	struct rca_fd_data *this = self->data;
	const struct relay_client_fd_data *args = initargs;
	if (this->fd < 0) {
		return false;
	}
	this->fd = args->fd;
	this->owns_fd = args->owns;
	return true;
}

static void rca_fd_destroy(struct relay_client *self)
{
	struct rca_fd_data *this = self->data;
	if (this->owns_fd) {
		close(this->fd);
	}
}

static bool rca_fd_send(struct relay_client *self, const void *buf, size_t length)
{
	struct rca_fd_data *this = self->data;
	const char *ptr = buf;
	while (length) {
		ssize_t bytes = write(this->fd, ptr, length);
		if (bytes == -1 || bytes == 0) {
			return false;
		}
		length -= bytes;
		ptr += bytes;
	}
	return true;
}

static bool rca_fd_recv(struct relay_client *self, void *buf, size_t length)
{
	struct rca_fd_data *this = self->data;
	char *ptr = buf;
	while (length) {
		ssize_t bytes = read(this->fd, ptr, length);
		if ((bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) || bytes == 0) {
			return false;
		}
		length -= bytes;
		ptr += bytes;
	}
	return true;
}

const struct relay_client_adapter relay_client_fd_adapter = {
	.init = rca_fd_init,
	.destroy = rca_fd_destroy,
	.send = rca_fd_send,
	.recv = rca_fd_recv,
	.instdata_size = sizeof(struct rca_fd_data)
};

/* I/O */

static bool relay_client_write(struct relay_client *self, const void *buf, const size_t length)
{
	if (self->failed) {
		debug_log("Attempted to write to relay client while in failed state\n");
		return false;
	}
	debug_log_v("Writing %lu bytes\n", (long) length);
	bool res =  self->adapter->send(self, buf, length);
	debug_log_v("Written %lu bytes\n", (long) length);
	if (!res) {
		debug_log("Write failed (errno=%d, bytes=%lu)\n", errno, (long) length);
	}
	return res;
}

static bool relay_client_read(struct relay_client *self, void *buf, size_t length)
{
	if (self->failed) {
		debug_log("Attempted to read from relay client while in failed state\n");
		return false;
	}
#if defined DEBUG_VERBOSE_relay
	int waiting;
	ioctl(self->fd, FIONREAD, &waiting);
	debug_log_v("Reading %lu bytes (%lu available)\n", (long) length, (long) waiting);
#endif
	bool res = self->adapter->recv(self, buf, length);
	debug_log_v("Read %lu bytes\n", (long) length);
	if (!res) {
		debug_log("Read failed (errno=%d, bytes=%lu)\n", errno, (long) length);
	}
	return res;
}

/* Convenience constructors */

bool relay_client_init_socket(struct relay_client *self, const char *local, const char *addr, const uint16_t port)
{
	struct relay_client_socket_data args = {
		.addr = addr,
		.port = port
	};
	return relay_client_init(self, local, &relay_client_socket_adapter, &args);
}

bool relay_client_init_fd(struct relay_client *self, const char *local, int fd, bool owns)
{
	struct relay_client_fd_data args = {
		.fd = fd,
		.owns = owns
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
	if (!self->adapter) {
		return;
	}
	self->adapter->destroy(self);
	self->adapter = NULL;
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
		debug_log("Attempted to send packet larger (%lu) than client MTU (%lu)\n", (long) total_length, (long) self->mtu);
		return false;
	}
	return relay_client_write(self, packet, total_length);
}

/* Reading */

static ssize_t relay_client_read_hdr(struct relay_client *self)
{
	if (!self->has_header) {
		debug_log_v("Reading header\n");
		if (!relay_client_read(self, &self->hdr, sizeof(self->hdr))) {
			return -1;
		}
		self->has_header = true;
	}
	return ntohl(self->hdr.length);
}

static bool relay_client_read_payload(struct relay_client *self, struct relay_packet_serial *ps)
{
	if (!self->has_header) {
		return false;
	}
	debug_log_v("Reading payload\n");
	if (!relay_client_read(self, ps->data, ntohl(self->hdr.length))) {
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
		return NULL;
	}
	struct relay_packet_serial *ps;
	size_t in_length = sizeof(self->hdr) + data_length;
	/* MTU/max-size check */
	if (in_length > self->mtu) {
		self->failed |= RCF_RECV_TOO_LARGE;
		debug_log("Attempted to receive packet larger (%lu) than client MTU (%lu)\n", (long) in_length, (long) self->mtu);
		return NULL;
	}
	/* Add extra byte for null-terminator */
	size_t alloc_length = sizeof(*ps) + data_length + 1;
	ps = malloc(alloc_length);
	/* Add null-terminator to after received data block */
	ps->data[data_length] = 0;
	if (!relay_client_read_payload(self, ps)) {
		free(ps);
		return NULL;
	}
	return ps;
}

struct relay_packet *relay_client_recv_packet(struct relay_client *self)
{
	ssize_t data_length = relay_client_read_hdr(self);
	if (data_length == -1) {
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
		debug_log("Attempted to receive packet larger (%lu) than client MTU (%lu)\n", (long) in_length, (long) self->mtu);
		return NULL;
	}
	/* Add extra byte for null-terminator */
	size_t alloc_length = sizeof(*tuple) + data_length + 1;
	tuple = malloc(alloc_length);
	/* Add null-terminator to after received data block */
	tuple->ps.data[data_length] = 0;
	if (!relay_client_read_payload(self, &tuple->ps)) {
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
