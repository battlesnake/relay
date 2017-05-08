#if defined DEMO_relay_client

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

#define log(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)

int main(int argc, char *argv[])
{
	if (argc < 4) {
		log("Syntax: %s <addr> <port> <name> [initial-message]", argv[0]);
		return 1;
	}
	const char *addr = argv[1];
	const char *port = argv[2];
	const char *name = argv[3];
	if (port == 0) {
		log("Invalid port: %s", argv[2]);
		return 1;
	}
	struct relay_client client;
	if (!relay_client_init_socket(&client, name, addr, port) != 0) {
		log("Failed to connect to %s:%s", addr, port);
		return 2;
	}
	for (int i = 4; i < argc; i++) {
		if (!relay_client_send_packet(&client, "DATA", "echo", argv[i], -1)) {
			log("Send failed");
			relay_client_destroy(&client);
			return 3;
		}
		log("Packet sent to 'echo': %s", argv[i]);
	}
	while (true) {
		struct relay_packet *p;
		if (!relay_client_recv_packet(&client, &p)) {
			log("Receive failed");
			relay_client_destroy(&client);
			return 3;
		}
		if (p == NULL) {
			break;
		}
		if (strncmp(p->type, "DATA", 4) == 0) {
			log("(packet echoed)");
			if (!relay_client_send_packet(&client, "ECHO", p->remote, p->data, p->length)) {
				log("Send failed");
				relay_client_destroy(&client);
				return 3;
			}
		}
		log("Packet of type '%s' received at <%s> from <%s>: %s", p->type, p->local, p->remote, p->data);
		free(p);
	}
	relay_client_destroy(&client);
	return 0;
}
#endif
