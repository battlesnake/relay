#if defined prog_relay_send

/* Sends STDIN as a single message */
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

int main(int argc, char *argv[])
{
	if (argc < 6) {
		fprintf(stderr, "Syntax: %s <addr> <port> <local> <remote> <type>\n", argv[0]);
		return 1;
	}
	const char *addr = argv[1];
	const char *port = argv[2];
	const char *local = argv[3];
	const char *remote = argv[4];
	const char *type = argv[5];
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
	struct relay_client client;
	if (!relay_client_init_socket(&client, local, addr, port) != 0) {
		fprintf(stderr, "Failed to connect to %s:%s\n", addr, port);
		return 2;
	}
	size_t length = 0;;
	size_t capacity = 16 << 20;
	char *buf = malloc(capacity);
	ssize_t in;
	while ((in = read(STDIN_FILENO, buf + length, capacity - length))) {
		if (in == -1) {
			fprintf(stderr, "Failed to read data (errno=%d)\n", errno);
			return 3;
		}
		length += in;
		if (length == capacity) {
			capacity += (capacity >> 1);
			buf = realloc(buf, capacity);
		}
	}
	if (!relay_client_send_packet(&client, type, remote, buf, length)) {
		fprintf(stderr, "Send failed\n");
		relay_client_destroy(&client);
		return 4;
	}
	fprintf(stderr, "%zu bytes sent to %s\n", length, remote);
	relay_client_destroy(&client);
	return 0;
}
#endif
