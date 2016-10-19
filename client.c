/* Echoes whatever the server sends, back to the server */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include "ctcp/socket.h"

struct packet {
	const char *type;
	const char *endpoint;
	size_t length;
	const char *data;
};

struct __attribute__((__packed__)) packet_serial {
	char type[4];
	char endpoint[8];
	uint32_t length;
	char data[];
};

static void make_packet(struct packet *out, const char *type, const char *endpoint, const char *data, ssize_t length)
{
	memset(out, 0, sizeof(*out));
	out->type = type;
	out->endpoint = endpoint;
	if (length < 0) {
		length = strlen(data);
	}
	out->length = length;
	out->data = data;
}

/* Creates serialised packet, original can be freed after */
static struct packet_serial *serialise_packet(const struct packet *in, size_t *out_size)
{
	struct packet_serial *p;
	const size_t out_len = sizeof(*p) + in->length;
	p = malloc(out_len);
	strncpy(p->type, in->type, sizeof(p->type));
	strncpy(p->endpoint, in->endpoint, sizeof(p->endpoint));
	p->length = htonl(in->length);
	memcpy(p->data, in->data, in->length);
	*out_size = out_len;
	return p;
}

/* Creates deserialised packet, pointing to fields within serial packet */
static bool deserialise_packet(struct packet *out, const struct packet_serial *in, const size_t in_length)
{
	out->type = in->type;
	out->endpoint = in->endpoint;
	out->length = ntohl(in->length);
	out->data = in->data;
	return in_length == (sizeof(*in) + out->length);
}

struct socket_client client;

static bool send_packet(const char *type, const char *endpoint, const char *data, const size_t length)
{
	struct packet p;
	make_packet(&p, type, endpoint, data, length);
	size_t serial_length;
	struct packet_serial *s = serialise_packet(&p, &serial_length);
	bool res = socket_client_send(&client, s, serial_length);
	free(s);
	return res;
}

static struct packet *recv_packet()
{
	struct packet_serial hdr;
	if (!socket_client_recv(&client, &hdr, sizeof(hdr))) {
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
		struct packet p;
		struct packet_serial ps;
	} *tuple;
	size_t in_length = sizeof(hdr) + data_length;
	/* Add extra byte for null-terminator */
	size_t alloc_length = sizeof(*tuple) + data_length + 1;
	tuple = malloc(alloc_length);
	/* Add null-terminator to after received data block */
	tuple->ps.data[alloc_length - 1] = 0;
	memcpy(&tuple->ps, &hdr, sizeof(hdr));
	if (!socket_client_recv(&client, tuple->ps.data, data_length)) {
		free(tuple);
		return NULL;
	}
	deserialise_packet(&tuple->p, &tuple->ps, in_length);
	return &tuple->p;
}

void socket_error(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
	if (argc < 4) {
		fprintf(stderr, "Syntax: %s <addr> <port> <name> [initial-message]\n", argv[0]);
		return 1;
	}
	const char *addr = argv[1];
	const unsigned port = atoi(argv[2]);
	const char *name = argv[3];
	if (port == 0) {
		fprintf(stderr, "Invalid port: %s\n", argv[2]);
		return 1;
	}
	if (!socket_client_init(&client, addr, port)) {
		fprintf(stderr, "Failed to connect to %s:%d\n", addr, port);
		return 2;
	}
	setsockopt_nodelay(client.fd);
	setsockopt_keepalive(client.fd);
	if (!send_packet("AUTH", "", name, -1)) {
		fprintf(stderr, "Login failed\n");
		socket_client_destroy(&client);
		return 3;
	}
	for (int i = 4; i < argc; i++) {
		if (!send_packet("DATA", "echo", argv[i], -1)) {
			fprintf(stderr, "Send failed\n");
			socket_client_destroy(&client);
			return 3;
		}
		fprintf(stderr, "Packet sent: %s\n", argv[i]);
	}
	while (true) {
		struct packet *p = recv_packet();
		if (p == NULL) {
			fprintf(stderr, "Receive failed\n");
			socket_client_destroy(&client);
			return 3;
		}
		if (strncmp(p->type, "DATA", 4) == 0) {
			fprintf(stderr, "ECHO sent\n");
			if (!send_packet("ECHO", "echo", p->data, p->length)) {
				fprintf(stderr, "Send failed\n");
				socket_client_destroy(&client);
				return 3;
			}
		} else if (strncmp(p->type, "ECHO", 4) == 0) {
			fprintf(stderr, "Echo received from %4.4s: %*s\n", p->endpoint, (int) p->length, p->data);
		} else {
			fprintf(stderr, "Unknown packet received: \"%4.4s\"\n", p->type);
		}
	}
	return 0;
}
