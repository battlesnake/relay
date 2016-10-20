#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "relay_packet.h"

void relay_make_packet(struct relay_packet *out, const char *type, const char *endpoint, const char *data, ssize_t length)
{
	memset(out, 0, sizeof(*out));
	out->type = type;
	out->endpoint = endpoint;
	if (length < 0) {
		length = strlen(data);
	}
	out->length = length;
	out->data = data;
}

/* Creates serialised packet, original can be freed after */
struct relay_packet_serial *relay_serialise_packet(const struct relay_packet *in, size_t *out_size)
{
	struct relay_packet_serial *p;
	const size_t out_len = sizeof(*p) + in->length;
	p = malloc(out_len);
	strncpy(p->type, in->type, sizeof(p->type));
	strncpy(p->endpoint, in->endpoint, sizeof(p->endpoint));
	p->length = htonl(in->length);
	memcpy(p->data, in->data, in->length);
	*out_size = out_len;
	return p;
}

/* Creates deserialised packet, pointing to fields within serial packet */
bool relay_deserialise_packet(struct relay_packet *out, const struct relay_packet_serial *in, const size_t in_length)
{
	if (in_length < sizeof(*in)) {
		return false;
	}
	out->type = in->type;
	out->endpoint = in->endpoint;
	out->length = ntohl(in->length);
	out->data = in->data;
	return in_length == (sizeof(*in) + out->length);
}

