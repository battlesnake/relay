#pragma once
#include <cstd/std.h>
#include <cstd/unix.h>
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
	char local[RELAY_ENDPOINT_LENGTH + 1];
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

enum rca_recv_result {
	rcarr_success = 0,
	rcarr_fail = 1,
	rcarr_eof = 2
};

typedef bool relay_client_adapter_init(struct relay_client *self, const void *initargs);
typedef void relay_client_adapter_destroy(struct relay_client *self);
typedef bool relay_client_adapter_send(struct relay_client *self, const void *buf, size_t length);
typedef enum rca_recv_result relay_client_adapter_recv(struct relay_client *self, void *buf, size_t length);

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
 * "local" is name to authenticate as.
 * Can be NULL to skip authentication.
 */
bool relay_client_init(struct relay_client *self, const char *local, const struct relay_client_adapter *adapter, const void *args);

/* Destructor */
void relay_client_destroy(struct relay_client *self);


/* Various ways to send a packet.  */

/*
 * Packet payload is contents of NULL-terminated string "text", sent without the
 * null terminator (which is added implicitly anyway by recv_packet).
 */
bool relay_client_send_text(struct relay_client *self, const char *type, const char *remote, const char *text);

/* Constructs a packet from the given type/endpoint/data and sends it */
bool relay_client_send_packet(struct relay_client *self, const char *type, const char *remote, const void *data, const size_t length);

/* Serialises a packet and sends it (sender name in packet is not altered) */
bool relay_client_send_packet2(struct relay_client *self, const struct relay_packet *packet);

/* Sends a serialised packet (sender name in packet is not altered) */
bool relay_client_send_packet3(struct relay_client *self, const struct relay_packet_serial *packet, size_t total_length);


/* Various ways to receive a packet */

/*
 * Receives a packet.
 *
 * Not re-entrant, call only from one thread unless protected by mutex.
 *
 * Returns false on error, true with *out == NULL on EOF
 */
bool relay_client_recv_packet(struct relay_client *self, struct relay_packet **out);
bool relay_client_recv_serialised_packet(struct relay_client *self, struct relay_packet_serial **out);

/*
 * Uses recv_packet, explodes packet - any of type/endpoint/buf may be NULL.
 *
 * Returns length of packet payload even if buf was NULL or was too small to
 * receive the complete payload.
 *
 * Returns false on error, returns true with buf_length == -1 on EOF.
 */
bool relay_client_recv_data(struct relay_client *self, char *type, char *remote, char *local, char *buf, size_t buf_size, ssize_t *buf_length);


/*** Some useful adapters ***/


/* Socket adapter */

struct relay_client_socket_data {
	const char *addr;
	const char *port;
};

extern const struct relay_client_adapter relay_client_socket_adapter;

bool relay_client_init_socket(struct relay_client *self, const char *local, const char *addr, const char *port);


/* File-desciptor adapter */

struct relay_client_fd_data {
	int fd;
	bool owns;
	bool auth_needed;
};

extern const struct relay_client_adapter relay_client_fd_adapter;

bool relay_client_init_fd(struct relay_client *self, const char *local, int fd, bool owns, bool auth_needed);
