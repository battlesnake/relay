#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctcp/socket.h>
#include <ctcp/select.h>
#include "relay_packet.h"

#define RELAY_CLIENT_FD 1
#define RELAY_CLIENT_TCP 2

struct relay_client {
	int type;
	FILE *file;
	struct socket_client socket;
	bool has_header;
	struct relay_packet_serial hdr;
};

#define RCI_CONNECT_FAILED 1
#define RCI_AUTH_FAILED 2

int relay_client_init(struct relay_client *self, const char *name, const char *addr, const uint16_t port);
int relay_client_init_fd(struct relay_client *self, const char *name, int fd, const char *mode);
void relay_client_destroy(struct relay_client *self);
/* Send over socket */
bool relay_client_send_packet(struct relay_client *self, const char *type, const char *endpoint, const char *data, const size_t length);
bool relay_client_send_packet2(struct relay_client *self, const struct relay_packet *packet);
bool relay_client_send_packet3(struct relay_client *self, const struct relay_packet_serial *packet, const size_t total_length);
/* Not re-entrant */
struct relay_packet *relay_client_recv_packet(struct relay_client *self);
