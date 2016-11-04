#pragma once
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define RELAY_TYPE_LENGTH 4
#define RELAY_ENDPOINT_LENGTH 8

/* Contains pointers to data, does not store inside the struct */
struct relay_packet {
	char type[RELAY_TYPE_LENGTH + 1];
	char endpoint[RELAY_ENDPOINT_LENGTH + 1];
	size_t length;
	char *data;
};

/* Wire-format: all data is packaged in the struct */
struct __attribute__((__packed__)) relay_packet_serial_hdr {
	char type[RELAY_TYPE_LENGTH];
	char endpoint[RELAY_ENDPOINT_LENGTH];
	uint32_t length;
};

/* Wire-format: all data is packaged in the struct */
struct __attribute__((__packed__)) relay_packet_serial {
	struct relay_packet_serial_hdr header;
	char data[];
};

/* Serialise data (relay_make_packet+relay_serialise_packet) */
struct relay_packet_serial *relay_make_serialised_packet(const char *type, const char *endpoint, const char *data, ssize_t length, size_t *out_size);

/* Encode data into packet (store pointer to data, don't copy in) */
void relay_make_packet(struct relay_packet *out, const char *type, const char *endpoint, char *data, ssize_t length);

/* Explode a packet, returns actual length of data, even if buf was too small */
size_t relay_explode_packet(struct relay_packet *packet, char *type, char *endpoint, char *buf, size_t buf_size);
size_t relay_explode_serialised_packet(struct relay_packet_serial *packet, char *type, char *endpoint, char *buf, size_t buf_size);

/* Number of bytes required for serialised packet */
size_t relay_serialised_packet_size(size_t in_size);

/* If out is NULL, mallocs serialised packet.  In either case, original can be freed after */
struct relay_packet_serial *relay_serialise_packet(struct relay_packet_serial *out, const struct relay_packet *in, size_t *out_size);

/* Creates deserialised packet, pointing to fields within serial packet */
bool relay_deserialise_packet(struct relay_packet *out, struct relay_packet_serial *in, const size_t in_length);
