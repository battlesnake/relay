#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "relay_packet.h"
#include "relay_client.h"
#include "debug.h"

size_t relay_client_mtu = 1L << 31;

static void __attribute__((__noreturn__)) invalid_type()
{
	debug_log("Invalid type, is client initialised?");
	abort();
}

/* POSIX: Write entire buffer */
static bool buf_write(int fd, const void *buf, size_t length)
{
	const char *ptr = buf;
	while (length) {
		ssize_t bytes = write(fd, ptr, length);
		if (bytes == -1 || bytes == 0) {
			return false;
		}
		length -= bytes;
		ptr += bytes;
	}
	return true;
}

/* POSIX: Read entire buffer */
static bool buf_read(int fd, void *buf, size_t length)
{
	char *ptr = buf;
	while (length) {
		ssize_t bytes = read(fd, ptr, length);
		if (bytes == -1 || bytes == 0) {
			return false;
		}
		length -= bytes;
		ptr += bytes;
	}
	return true;
}

static bool relay_client_do_write(struct relay_client *self, const void *buf, const size_t length)
{
	switch (self->type) {
	case RELAY_CLIENT_FD: return buf_write(self->fd, buf, length);
	case RELAY_CLIENT_TCP: return socket_client_send(&self->socket, buf, length);
	default: invalid_type();
	}
}

static bool relay_client_do_read(struct relay_client *self, void *buf, size_t length)
{
	switch (self->type) {
	case RELAY_CLIENT_FD: return buf_read(self->fd, buf, length);
	case RELAY_CLIENT_TCP: return socket_client_peek(&self->socket, buf, length) && socket_client_recv(&self->socket, buf, length);
	default: invalid_type();
	}
}

static bool relay_client_write(struct relay_client *self, const void *buf, const size_t length)
{
	if (self->failed) {
		debug_log("Attempted to write to relay client while in failed state\n");
		return false;
	}
	debug_log_v("Writing %lu bytes\n", (long) length);
	bool res = relay_client_do_write(self, buf, length);
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
	bool res = relay_client_do_read(self, buf, length);
	debug_log_v("Read %lu bytes\n", (long) length);
	if (!res) {
		debug_log("Read failed (errno=%d, bytes=%lu)\n", errno, (long) length);
	}
	return res;
}

static void relay_client_init_base(struct relay_client *self, int type)
{
	self->failed = 0;
	self->mtu = relay_client_mtu;
	self->type = type;
}

int relay_client_init(struct relay_client *self, const char *name, const char *addr, const uint16_t port)
{
	int res = 0;
	relay_client_init_base(self, RELAY_CLIENT_TCP);
	if (!socket_client_init(&self->socket, addr, port, NULL)) {
		res = RCI_CONNECT_FAILED;
		goto fail;
	}
	setsockopt_nodelay(self->socket.fd);
	setsockopt_keepalive(self->socket.fd);
	if (name && !relay_client_send_packet(self, "AUTH", "", name, -1)) {
		res = RCI_AUTH_FAILED;
		goto fail2;
	}
	return 0;
fail2:
	socket_client_destroy(&self->socket);
fail:
	self->failed |= RCF_INIT;
	debug_log("Failed to initialise relay client, error code %d\n", res);
	return res;
}

int relay_client_init_fd(struct relay_client *self, const char *name, int fd)
{
	int res = 0;
	relay_client_init_base(self, RELAY_CLIENT_FD);
	self->fd = fd;
	if (name && !relay_client_send_packet(self, "AUTH", "", name, -1)) {
		res = RCI_AUTH_FAILED;
		goto fail;
	}
	return 0;
fail:
	self->failed |= RCF_INIT;
	debug_log("Failed to initialise relay client, error code %d\n", res);
	return res;
}

void relay_client_destroy(struct relay_client *self)
{
	switch (self->type) {
	case 0: return;
	case RELAY_CLIENT_FD: close(self->fd); break;
	case RELAY_CLIENT_TCP: socket_client_destroy(&self->socket); break;
	default: invalid_type();
	}
	self->type = 0;
}

bool relay_client_send_text(struct relay_client *self, const char *type, const char *endpoint, const char *text)
{
	return relay_client_send_packet(self, type, endpoint, text, strlen(text));
}

bool relay_client_send_packet(struct relay_client *self, const char *type, const char *endpoint, const char *data, const size_t length)
{
	struct relay_packet p;
	relay_make_packet(&p, type, endpoint, (char *) data, length);
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

bool relay_client_send_packet3(struct relay_client *self, const struct relay_packet_serial *packet, const size_t total_length)
{
	if (total_length > self->mtu) {
		self->failed |= RCF_SEND_TOO_LARGE;
		debug_log("Attempted to send packet larger (%lu) than client MTU (%lu)\n", (long) total_length, (long) self->mtu);
		return false;
	}
	return relay_client_write(self, packet, total_length);
}

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
