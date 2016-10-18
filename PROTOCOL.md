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

	Field	Bytes	Type		Description
	Type	4	char[4]		Packet type
	Target	16	char[16]	Terminal endpoint name (null-padded)
	Origin	16	char[16]	Originating endpoint name (null-padded)
	Length	4	u32		N = Payload length (1+31-bit integer)
	Data	N	u8[N]		Payload

\* = High-order bit indicates that packet was received from relay server (vs. piped in from another part of the program).
This is necessary since the server swaps remote/local fields, as otherwise there is no way to tell which packets are generated locally and which came from outside.

# Behaviour

The relay will not send a message to the name from which it originated.
If a node sends a message addressed to itself, it will not be sent to ANY nodes (including others with the same name).
If a node sends a wildcard-addressed message, the node will be excluded from the result of the wildcard search.
