#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "relay_packet.h"

struct relay_packet_serial *relay_make_serialised_packet(const char *type, const char *remote, const char *local, const char *data, ssize_t length, size_t *out_size)
{
	/* TODO: Optimised version which builds relay_packet_serial directly */
	struct relay_packet packet;
	relay_make_packet(&packet, type, remote, local, (char *) data, length);
	return relay_serialise_packet(NULL, &packet, out_size);
}

void relay_make_packet(struct relay_packet *out, const char *type, const char *remote, const char *local, char *data, ssize_t length)
{
	memset(out, 0, sizeof(*out));
	/* Type */
	if (type) {
		strncpy(out->type, type, RELAY_TYPE_LENGTH);
	}
	/* Target */
	if (remote) {
		strncpy(out->remote, remote, RELAY_ENDPOINT_LENGTH);
	}
	/* Origin */
	if (local) {
		strncpy(out->local, local, RELAY_ENDPOINT_LENGTH);
	}
	/* Length */
	if (length < 0) {
		length = data == NULL ? 0 : strlen(data);
	}
	out->length = length;
	/* Data */
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
	strncpy(out->header.remote, in->remote, RELAY_ENDPOINT_LENGTH);
	strncpy(out->header.local, in->local, RELAY_ENDPOINT_LENGTH);

	out->header.length = htonl(in->length);
	memcpy(out+1, in->data, in->length);
	*out_size = out_len;

	return out;
}

bool relay_deserialise_packet(struct relay_packet *out, struct relay_packet_serial *in, const size_t in_length)
{
	if (in_length < sizeof(*in)) {
		return false;
	}

	out->type[RELAY_TYPE_LENGTH] = 0;
	strncpy(out->type, in->header.type, RELAY_TYPE_LENGTH);

	out->remote[RELAY_ENDPOINT_LENGTH] = 0;
	strncpy(out->remote, in->header.remote, RELAY_ENDPOINT_LENGTH);

	out->local[RELAY_ENDPOINT_LENGTH] = 0;
	strncpy(out->local, in->header.local, RELAY_ENDPOINT_LENGTH);

	out->length = ntohl(in->header.length);
	out->data = in->data;
	return in_length == (sizeof(*in) + out->length);
}

size_t relay_explode_packet(struct relay_packet *packet, char *type, char *remote, char *local, char *buf, size_t buf_size)
{
	if (packet == NULL) {
		return -1;
	}
	if (type) {
		strcpy(type, packet->type);
	}
	if (remote) {
		strcpy(remote, packet->remote);
	}
	if (local) {
		strcpy(local, packet->local);
	}
	if (buf) {
		size_t bytes = packet->length + 1;
		if (bytes > buf_size) {
			bytes = buf_size;
		}
		memcpy(buf, packet->data, bytes);
	}
	return packet->length;
}

size_t relay_explode_serialised_packet(struct relay_packet_serial *packet, char *type, char *remote, char *local, char *buf, size_t buf_size)
{
	if (packet == NULL) {
		return -1;
	}
	if (type) {
		int len = strnlen(packet->header.type, 4);
		memcpy(type, packet->header.type, len);
		type[len] = 0;
	}
	if (remote) {
		int len = strnlen(packet->header.remote, 4);
		memcpy(remote, packet->header.remote, len);
		remote[len] = 0;
	}
	if (local) {
		int len = strnlen(packet->header.local, 4);
		memcpy(local, packet->header.local, len);
		local[len] = 0;
	}
	size_t length = ntohl(packet->header.length);
	if (buf) {
		size_t bytes = length + 1;
		if (bytes > buf_size) {
			bytes = buf_size;
		}
		memcpy(buf, packet->data, bytes);
	}
	return length;
}
