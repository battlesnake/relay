#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "relay_packet.h"
#include "relay_client.h"
#include "relay_pipe.h"

#if defined DEMO_relay_pipe

static bool tap(struct relay_packet **packet)
{
	struct relay_packet *p = *packet;
	if (p->length > 5) {
		fprintf(stderr, "TAP: Length=%lu, allowing\n", (long) p->length);
		return true;
	} else if (p->length == 5) {
		fprintf(stderr, "TAP: Length=%lu, editing\n", (long) p->length);
		strcpy(p->data, "Big");
		p->length = 3;
		return true;
	} else {
		fprintf(stderr, "TAP: Length=%lu, dropping\n", (long) p->length);
		return false;
	}
}

int main()
{
	union {
		int fd[2];
		struct {
			int read;
			int write;
		};
	} pin, pout;
	if (pipe(pin.fd) || pipe(pout.fd)) {
		fprintf(stderr, "Failed to create pipes\n");
		return 1;
	}
	/* Init */
	struct relay_client in;
	struct relay_client out;
	struct relay_pipe mid;
	if (relay_client_init_fd(&in, NULL, pin.write) ||
			relay_client_init_fd(&out, NULL, pout.read) ||
			relay_pipe_init(&mid, pin.read, pout.write, tap)) {
		fprintf(stderr, "Failed to create relay interfaces\n");
		return 2;
	}
	/* Run */
	if (!relay_client_send_text(&in, "TEST", "Potato", "Tiny")) {
		fprintf(stderr, "Failed to send\n");
		return 3;
	}
	if (!relay_client_send_text(&in, "TEST", "Potato", "Large")) {
		fprintf(stderr, "Failed to send\n");
		return 4;
	}
	if (!relay_client_send_text(&in, "TEST", "Potato", "Massive")) {
		fprintf(stderr, "Failed to send\n");
		return 5;
	}
	struct relay_packet *p;
	if (!(p = relay_client_recv_packet(&out))) {
		fprintf(stderr, "Failed to receive\n");
		return 6;
	}
	if (strcmp(p->data, "Big")) {
		fprintf(stderr, "Wrong data received: %s\n", p->data);
		return 6;
	}
	free(p);
	if (!(p = relay_client_recv_packet(&out))) {
		fprintf(stderr, "Failed to receive\n");
		return 7;
	}
	if (strcmp(p->data, "Massive")) {
		fprintf(stderr, "Wrong data received: %s\n", p->data);
		return 7;
	}
	free(p);
	/* Deinit */
	relay_pipe_destroy(&mid);
	relay_client_destroy(&in);
	relay_client_destroy(&out);
	fprintf(stderr, "Test completed\n");
	return 0;
}

#endif
