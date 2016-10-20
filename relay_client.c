#include <string.h>
#include <stdlib.h>
#include "relay_client.h"

int relay_client_init(struct relay_client *self, const char *name, const char *addr, const uint16_t port)
{
	if (!socket_client_init(&self->socket, addr, port, NULL)) {
		return RCI_CONNECT_FAILED;
	}
	setsockopt_nodelay(self->socket.fd);
	setsockopt_keepalive(self->socket.fd);
	if (!relay_client_send_packet(self, "AUTH", "", name, -1)) {
		socket_client_destroy(&self->socket);
		return RCI_AUTH_FAILED;
	}
}

void relay_client_destroy(struct relay_client *self)
{
	socket_client_destroy(&self->socket);
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
	return socket_client_send(&self->socket, packet, total_length);
}

struct relay_packet *relay_client_recv_packet(struct relay_client *self)
{
	struct relay_packet_serial hdr;
	if (!socket_client_recv(&self->socket, &hdr, sizeof(hdr))) {
		return NULL;
	}
	size_t data_length = ntohl(hdr.length);
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
	size_t in_length = sizeof(hdr) + data_length;
	/* Add extra byte for null-terminator */
	size_t alloc_length = sizeof(*tuple) + data_length + 1;
	tuple = malloc(alloc_length);
	/* Add null-terminator to after received data block */
	tuple->ps.data[data_length] = 0;
	memcpy(&tuple->ps, &hdr, sizeof(hdr));
	if (!socket_client_recv(&self->socket, tuple->ps.data, data_length)) {
		free(tuple);
		return NULL;
	}
	relay_deserialise_packet(&tuple->p, &tuple->ps, in_length);
	return &tuple->p;
}

struct relay_packet *relay_client_try_recv_packet(struct relay_client *self)
{
	struct relay_packet_serial hdr;
	if (!socket_client_peek(&self->socket, &hdr, sizeof(hdr))) {
		return NULL;
	}
	const size_t data_length = ntohl(hdr.length);
	const size_t recv_length = sizeof(hdr) + data_length;
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
	/* Add extra byte for null-terminator */
	const size_t alloc_length = sizeof(*tuple) + data_length + 1;
	tuple = malloc(alloc_length);
	/* Add null-terminator to after received data block */
	tuple->ps.data[data_length] = 0;
	if (!socket_client_peek(&self->socket, &tuple->ps, recv_length)) {
		free(tuple);
		return NULL;
	}
	if (!socket_client_recv(&self->socket, &tuple->ps, recv_length)) {
		free(tuple);
		return NULL;
	}
	relay_deserialise_packet(&tuple->p, &tuple->ps, recv_length);
	return &tuple->p;
}
