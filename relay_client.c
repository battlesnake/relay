#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "relay_client.h"

static void __attribute__((__noreturn__)) invalid_type()
{
	abort();
}

static bool relay_client_write(struct relay_client *self, const void *buf, const size_t length)
{
	switch (self->type) {
	case RELAY_CLIENT_FD: return fwrite(buf, 1, length, self->file) == 1;
	case RELAY_CLIENT_TCP: return socket_client_send(&self->socket, buf, length);
	default: invalid_type();
	}
}

static bool relay_client_read(struct relay_client *self, void *buf, size_t length)
{
	switch (self->type) {
	case RELAY_CLIENT_FD: return fread(buf, 1, length, self->file) == 1;
	case RELAY_CLIENT_TCP: return socket_client_peek(&self->socket, buf, length) && socket_client_recv(&self->socket, buf, length);
	default: invalid_type();
	}
}

int relay_client_init(struct relay_client *self, const char *name, const char *addr, const uint16_t port)
{
	self->type = RELAY_CLIENT_TCP;
	if (!socket_client_init(&self->socket, addr, port, NULL)) {
		return RCI_CONNECT_FAILED;
	}
	setsockopt_nodelay(self->socket.fd);
	setsockopt_keepalive(self->socket.fd);
	if (!relay_client_send_packet(self, "AUTH", "", name, -1)) {
		socket_client_destroy(&self->socket);
		return RCI_AUTH_FAILED;
	}
	return 0;
}

int relay_client_init_fd(struct relay_client *self, const char *name, int fd, const char *mode)
{
	self->type = RELAY_CLIENT_FD;
	self->file = fdopen(fd, mode);
	if (!self->file) {
		return RCI_CONNECT_FAILED;
	}
	if (!relay_client_send_packet(self, "AUTH", "", name, -1)) {
		return RCI_AUTH_FAILED;
	}
	return 0;
}

void relay_client_destroy(struct relay_client *self)
{
	switch (self->type) {
	case RELAY_CLIENT_FD: fclose(self->file); break;
	case RELAY_CLIENT_TCP: socket_client_destroy(&self->socket); break;
	default: invalid_type();
	}
}

bool relay_client_send_packet(struct relay_client *self, const char *type, const char *endpoint, const char *data, const size_t length)
{
	struct relay_packet p;
	relay_make_packet(&p, type, endpoint, data, length);
	return relay_client_send_packet2(self, &p);
}

bool relay_client_send_packet2(struct relay_client *self, const struct relay_packet *packet)
{
	size_t total_length;
	struct relay_packet_serial *s = relay_serialise_packet(packet, &total_length);
	bool res = relay_client_send_packet3(self, s, total_length);
	free(s);
	return res;
}

bool relay_client_send_packet3(struct relay_client *self, const struct relay_packet_serial *packet, const size_t total_length)
{
	return relay_client_write(self, packet, total_length);
}

static ssize_t relay_client_read_hdr(struct relay_client *self)
{
	if (!self->has_header) {
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
	if (!relay_client_read(self, ps->data, ntohl(self->hdr.length))) {
		return NULL;
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
		struct relay_packet p;
		struct relay_packet_serial ps;
	} *tuple;
	size_t in_length = sizeof(self->hdr) + data_length;
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
