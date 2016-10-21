#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "relay_packet.h"

void relay_make_packet(struct relay_packet *out, const char *type, const char *endpoint, const char *data, ssize_t length)
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

/* Creates serialised packet, original can be freed after */
struct relay_packet_serial *relay_serialise_packet(const struct relay_packet *in, size_t *out_size)
{
	struct relay_packet_serial *out;
	size_t out_len = sizeof(*out) + in->length;
	out = malloc(out_len);

	strncpy(out->type, in->type, RELAY_TYPE_LENGTH);
	strncpy(out->endpoint, in->endpoint, RELAY_ENDPOINT_LENGTH);

	out->length = htonl(in->length);
	memcpy(out->data, in->data, in->length);
	*out_size = out_len;

	return out;
}

/* Creates deserialised packet, pointing to fields within serial packet */
bool relay_deserialise_packet(struct relay_packet *out, const struct relay_packet_serial *in, const size_t in_length)
{
	if (in_length < sizeof(*in)) {
		return false;
	}

	out->type[RELAY_TYPE_LENGTH] = 0;
	strncpy(out->type, in->type, RELAY_TYPE_LENGTH);
	out->endpoint[RELAY_ENDPOINT_LENGTH] = 0;
	strncpy(out->endpoint, in->endpoint, RELAY_ENDPOINT_LENGTH);

	out->length = ntohl(in->length);
	out->data = in->data;
	return in_length == (sizeof(*in) + out->length);
}

