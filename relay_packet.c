#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "relay_packet.h"

struct relay_packet_serial *relay_make_serialised_packet(const char *type, const char *endpoint, const char *data, ssize_t length, size_t *out_size)
{
	/* TODO: Optimised version which builds relay_packet_serial directly */
	struct relay_packet packet;
	relay_make_packet(&packet, type, endpoint, (char *) data, length);
	return relay_serialise_packet(NULL, &packet, out_size);
}

void relay_make_packet(struct relay_packet *out, const char *type, const char *endpoint, char *data, ssize_t length)
{
	out->type[RELAY_TYPE_LENGTH] = 0;
	strncpy(out->type, type, RELAY_TYPE_LENGTH);
	out->endpoint[RELAY_ENDPOINT_LENGTH] = 0;
	strncpy(out->endpoint, endpoint, RELAY_ENDPOINT_LENGTH);

	if (length < 0) {
		length = strlen(data);
	}
	out->length = length;
	out->data = data;
}

size_t relay_serialised_packet_size(size_t in_size)
{
	return sizeof(struct relay_packet_serial_hdr) + in_size;
}

struct relay_packet_serial *relay_serialise_packet(struct relay_packet_serial *out, const struct relay_packet *in, size_t *out_size)
{
	size_t out_len = relay_serialised_packet_size(in->length);

	if (!out) {
		out = malloc(out_len);
	}

	strncpy(out->header.type, in->type, RELAY_TYPE_LENGTH);
	strncpy(out->header.endpoint, in->endpoint, RELAY_ENDPOINT_LENGTH);

	out->header.length = htonl(in->length);
	memcpy(out+1, in->data, in->length);
	*out_size = out_len;

	return (void *) out;
}

bool relay_deserialise_packet(struct relay_packet *out, struct relay_packet_serial *in, const size_t in_length)
{
	if (in_length < sizeof(*in)) {
		return false;
	}

	out->type[RELAY_TYPE_LENGTH] = 0;
	strncpy(out->type, in->header.type, RELAY_TYPE_LENGTH);
	out->endpoint[RELAY_ENDPOINT_LENGTH] = 0;
	strncpy(out->endpoint, in->header.endpoint, RELAY_ENDPOINT_LENGTH);

	out->length = ntohl(in->header.length);
	out->data = in->data;
	return in_length == (sizeof(*in) + out->length);
}
