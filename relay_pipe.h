#pragma once
#include "relay_client.h"

typedef bool relay_pipe_tap(struct relay_packet **packet);

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
};

#define RPF_INIT 1
#define RPF_THREAD 2

#define RPI_OPEN_INPUT_FAILED 1
#define RPI_OPEN_OUTPUT_FAILED 2
#define RPI_THREAD_FAILED 4
#define RPI_INIT_FAILED 8

int relay_pipe_init(struct relay_pipe *inst, int fd_in, int fd_out, relay_pipe_tap *tap);
void relay_pipe_destroy(struct relay_pipe *inst);
