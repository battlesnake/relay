# Description

Simple packet broadcasting server

# Protocol

## Flow

1. Client connects.

2. Client logs in by sending a packet with:

	{ type: 'AUTH', name: '', length: thisName.length, data: thisName }

   Multiple clients may connect with the same name.  A message sent to a particular name will be forwarded to all clients with that name (or to no clients if none are registered with the given name).

3. Client can send messages to any name and receive messages to its name:

## Packet format:

	Field	Bytes	Type	Description
	Type	4	char[4]	Packet type
	Name	8	char[8]	Other endpoint name (null-terminated)
	Length	4	u32	N = Payload length (unsigned 32-bit integer)
	Data	N	u8[N]	Payload
