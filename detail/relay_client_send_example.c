#if defined DEMO_relay_client_send

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
#include "../relay_packet.h"
#include "../relay_client.h"

static struct relay_client client;

int main(int argc, char *argv[])
{
	if (argc < 8) {
		fprintf(stderr, "Syntax: %s <addr> <port> <delay> <local> <remote> <type> [initial-message]\n", argv[0]);
		return 1;
	}
	const char *addr = argv[1];
	const char *port = argv[2];
	const int delay = atoi(argv[3]);
	const char *local = argv[4];
	const char *remote = argv[5];
	const char *type = argv[6];
	if (port == 0) {
		fprintf(stderr, "Invalid port: %s\n", argv[2]);
		return 1;
	}
	if (strlen(local) > RELAY_ENDPOINT_LENGTH) {
		fprintf(stderr, "Local endpoint name is too long\n");
		return 1;
	}
	if (strlen(remote) > RELAY_ENDPOINT_LENGTH) {
		fprintf(stderr, "Remote endpoint name is too long\n");
		return 1;
	}
	if (strlen(type) > RELAY_TYPE_LENGTH) {
		fprintf(stderr, "Packet type name is too long\n");
		return 1;
	}
	if (!relay_client_init_socket(&client, local, addr, port) != 0) {
		fprintf(stderr, "Failed to connect to %s:%s\n", addr, port);
		return 2;
	}
	sleep(delay);
	for (int i = 7; i < argc; i++) {
		if (!relay_client_send_packet(&client, type, remote, argv[i], -1)) {
			fprintf(stderr, "Send failed\n");
			relay_client_destroy(&client);
			return 3;
		}
		fprintf(stderr, "Packet sent to %s: %s\n", remote, argv[i]);
	}
	while (true) {
		struct relay_packet *p = relay_client_recv_packet(&client);
		if (p == NULL) {
			fprintf(stderr, "Receive failed\n");
			relay_client_destroy(&client);
			return 3;
		}
		fprintf(stderr, "Packet of type '%s' received to <%s> from <%s>: %s\n", p->type, p->local, p->remote, p->data);
		free(p);
	}
	return 0;
}
#endif
