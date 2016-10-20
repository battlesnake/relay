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
#include "relay_packet.h"
#include "relay_client.h"

static struct relay_client client;

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
	if (!relay_client_init(&client, name, addr, port)) {
		fprintf(stderr, "Failed to connect to %s:%d\n", addr, port);
		return 2;
	}
	for (int i = 4; i < argc; i++) {
		if (!relay_client_send_packet(&client, "DATA", "echo", argv[i], -1)) {
			fprintf(stderr, "Send failed\n");
			relay_client_destroy(&client);
			return 3;
		}
		fprintf(stderr, "Packet sent: %s\n", argv[i]);
	}
	while (true) {
		struct relay_packet *p = relay_client_recv_packet(&client);
		if (p == NULL) {
			fprintf(stderr, "Receive failed\n");
			relay_client_destroy(&client);
			return 3;
		}
		if (strncmp(p->type, "DATA", 4) == 0) {
			fprintf(stderr, "ECHO sent\n");
			if (!relay_client_send_packet(&client, "ECHO", p->endpoint, p->data, p->length)) {
				fprintf(stderr, "Send failed\n");
				relay_client_destroy(&client);
				return 3;
			}
		} else if (strncmp(p->type, "ECHO", 4) == 0) {
			fprintf(stderr, "Echo received from %4.4s: %.*s\n", p->endpoint, (int) p->length, p->data);
		} else {
			fprintf(stderr, "Unknown packet received: \"%4.4s\"\n", p->type);
		}
		free(p);
	}
	return 0;
}
