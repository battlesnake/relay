#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <ctcp/socket.h>
#include <ctcp/select.h>
#include "relay_packet.h"

/*
 * Why do we have separate "socket" and "file-descriptor" implementations, when
 * sockets are accessed via file-descriptors?  Convenience.  In C++, we'd use a
 * template class and RAII to make this all nice and easy, but in C we need to
 * be a little bit more messy in order to provide minimal-clutter polymorphism
 * here.  I don't like the extra indirection of accessing the adapter/data via
 * pointers, but this pattern should allow us to extend onto non-POSIX platforms
 * where we can't use such universal abstractions for I/O.
 */

/*
 * MTU global - each new client is configured with an MTU of the current value
 * of this when the constructor was called.
 */
extern size_t relay_client_mtu;

struct relay_client_adapter;

struct relay_client {
	/* Name of this endpoint */
	char name[RELAY_ENDPOINT_LENGTH + 1];
	/* Buffer for receiving packet header */
	bool has_header;
	struct relay_packet_serial_hdr hdr;
	/* MTU (packet size limit) for this client */
	size_t mtu;
	/* Error state */
	int failed;
	/* Polymorphism (adapter class + adapter instance data) */
	const struct relay_client_adapter *adapter;
	void *data;
};


/* Adapter interface specification */

typedef bool relay_client_adapter_init(struct relay_client *self, const void *initargs);
typedef void relay_client_adapter_destroy(struct relay_client *self);
typedef bool relay_client_adapter_send(struct relay_client *self, const void *buf, size_t length);
typedef bool relay_client_adapter_recv(struct relay_client *self, void *buf, size_t length);

struct relay_client_adapter {
	relay_client_adapter_init *init;
	relay_client_adapter_destroy *destroy;
	relay_client_adapter_send *send;
	relay_client_adapter_recv *recv;
	size_t instdata_size;
};


/* Fail bits */

#define RCF_INIT 1
#define RCF_SEND_TOO_LARGE 2
#define RCF_RECV_TOO_LARGE 4


/*
 * Constructor
 *
 * "name" is name to authenticate as.
 * Can be NULL to skip authentication.
 */
bool relay_client_init(struct relay_client *self, const char *name, const struct relay_client_adapter *adapter, const void *args);

/* Destructor */
void relay_client_destroy(struct relay_client *self);


/* Various ways to send a packet.  */

/*
 * Packet payload is contents of NULL-terminated string "text", sent without the
 * null terminator (which is added implicitly anyway by recv_packet).
 */
bool relay_client_send_text(struct relay_client *self, const char *type, const char *endpoint, const char *text);

/* Constructs a packet from the given type/endpoint/data and sends it */
bool relay_client_send_packet(struct relay_client *self, const char *type, const char *endpoint, const char *data, const size_t length);

/* Serialises a packet and sends it (sender name in packet is not altered) */
bool relay_client_send_packet2(struct relay_client *self, const struct relay_packet *packet);

/* Sends a serialised packet (sender name in packet is not altered) */
bool relay_client_send_packet3(struct relay_client *self, const struct relay_packet_serial *packet, const size_t total_length);


/* Various ways to receive a packet */

/*
 * Receives a packet.
 *
 * Not re-entrant, call only from one thread unless protected by mutex.
 */
struct relay_packet *relay_client_recv_packet(struct relay_client *self);
struct relay_packet_serial *relay_client_recv_serialised_packet(struct relay_client *self);

/*
 * Uses recv_packet, explodes packet - any of type/endpoint/buf may be NULL.
 *
 * Returns length of packet payload even if buf was NULL or was too small to
 * receive the complete payload.
 *
 * Returns -1 on error.
 */
ssize_t relay_client_recv_data(struct relay_client *self, char *type, char *endpoint, char *buf, size_t buf_size);


/*** Some useful adapters ***/


/* Socket adapter */

struct relay_client_socket_data {
	const char *addr;
	uint16_t port;
};

extern const struct relay_client_adapter relay_client_socket_adapter;

bool relay_client_init_socket(struct relay_client *self, const char *name, const char *addr, const uint16_t port);


/* File-desciptor adapter */

struct relay_client_fd_data {
	int fd;
	bool owns;
};

extern const struct relay_client_adapter relay_client_fd_adapter;

bool relay_client_init_fd(struct relay_client *self, const char *name, int fd, bool owns);
