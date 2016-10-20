#pragma once
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Contains pointers to strings/data, does not store inside the struct */
struct relay_packet {
	const char *type;
	const char *endpoint;
	size_t length;
	const char *data;
};

/* Wire-format: all data is packaged in the struct */
struct __attribute__((__packed__)) relay_packet_serial {
	char type[4];
	char endpoint[8];
	uint32_t length;
	char data[];
};

void relay_make_packet(struct relay_packet *out, const char *type, const char *endpoint, const char *data, ssize_t length);

/* Creates serialised packet, original can be freed after */
struct relay_packet_serial *relay_serialise_packet(const struct relay_packet *in, size_t *out_size);

/* Creates deserialised packet, pointing to fields within serial packet */
bool relay_deserialise_packet(struct relay_packet *out, const struct relay_packet_serial *in, const size_t in_length);
