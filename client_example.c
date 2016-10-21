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

#if defined DEMO_relay

static struct relay_client client;

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
	if (relay_client_init(&client, name, addr, port) != 0) {
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
		}
		fprintf(stderr, "Packet of type '%s' received from <%s>: %s\n", p->type, p->endpoint, p->data);
		free(p);
	}
	return 0;
}
#endif
