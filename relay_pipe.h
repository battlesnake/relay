#pragma once
#include "relay_client.h"

/*
 * Pipes packets from one FD to another FD, applying a filter/map function to
 * each packet, if a function was provided.
 */

typedef bool relay_pipe_tap(struct relay_packet **packet, void *misc);

struct relay_pipe {
	struct relay_client reader;
	struct relay_client writer;
	int fd_in;
	int fd_out;
	int fd_end;
	int ep;
	relay_pipe_tap *tap;
	pthread_t piper;
	int failed;
	void *misc;
};

#define RPI_INIT_FAILED 1
#define RPI_OPEN_INPUT_FAILED 2
#define RPI_OPEN_OUTPUT_FAILED 4
#define RPI_THREAD_FAILED 8

bool relay_pipe_init(struct relay_pipe *inst, int fd_in, int fd_out, bool owns, relay_pipe_tap *tap, void *misc);
void relay_pipe_destroy(struct relay_pipe *inst);
