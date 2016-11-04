#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include "relay_pipe.h"
#include "debug.h"

#define max2(a,b) ((a)>(b)?(a):(b))
#define max3(a,b,c) max2((a),max2((b),(c)))

static void *pipe_thread(struct relay_pipe *inst)
{
	struct epoll_event epev[2];
	debug_log("Pipe thread created\n");
	while (1) {
		int nfds = epoll_wait(inst->ep, epev, sizeof(epev)/sizeof(epev[0]), -1);
		if (nfds == -1) {
			debug_log("Pipe stopping due to error %d\n", errno);
			break;
		}
		if (nfds == 0) {
			continue;
		}

		bool exiting = false;
		for (size_t i = 0; i < sizeof(epev)/sizeof(epev[0]); i++) {
			if (epev[i].data.fd == inst->fd_end) {
				debug_log("Pipe stopping due to exit signal\n");
				exiting = true;
			}
		}
		if (exiting) {
			break;
		}

		struct relay_packet *packet = relay_client_recv_packet(&inst->reader);
		if (!packet) {
			debug_log("Pipe failed to receive packet\n");
			break;
		}
		if (!inst->tap || inst->tap(&packet, inst->misc)) {
			debug_log_v("Pipe accepted a packet\n");
			if (!relay_client_send_packet2(&inst->writer, packet)) {
				inst->failed |= RPI_THREAD_FAILED;
				break;
			}
		} else {
			debug_log_v("Pipe rejected a packet\n");
		}
		free(packet);
	}
	debug_log("Pipe thread exited\n");

	return NULL;
}

bool relay_pipe_init(struct relay_pipe *inst, int fd_in, int fd_out, bool owns, relay_pipe_tap *tap, void *misc)
{
	memset(inst, 0, sizeof(*inst));

	inst->misc = misc;

	inst->fd_in = fd_in;
	inst->fd_out = fd_out;

	inst->fd_end = eventfd(0, 0);
	if (inst->fd_end == -1) {
		goto fail;
	}

	inst->ep = epoll_create1(0);
	if (inst->ep == -1) {
		goto fail;
	}
	struct epoll_event epev[2] = {
		{ .events = EPOLLIN, .data = { .fd = inst->fd_in } },
		{ .events = EPOLLIN, .data = { .fd = inst->fd_end } }
	};
	for (size_t i = 0; i < sizeof(epev)/sizeof(epev[0]); i++) {
		if (epoll_ctl(inst->ep, EPOLL_CTL_ADD, epev[i].data.fd, &epev[i]) == -1) {
			goto fail;
		}
	}

	if (!relay_client_init_fd(&inst->reader, NULL, fd_in, owns)) {
		inst->failed |= RPI_OPEN_INPUT_FAILED;
		goto fail;
	}
	if (!relay_client_init_fd(&inst->writer, NULL, fd_out, owns)) {
		inst->failed |= RPI_OPEN_OUTPUT_FAILED;
		goto fail;
	}
	inst->tap = tap;

	if (pthread_create(&inst->piper, NULL, (void*(*)(void*)) pipe_thread, inst)) {
		inst->failed |= RPI_THREAD_FAILED;
		goto fail;
	}

	return true;
fail:
	inst->failed |= RPI_INIT_FAILED;
	relay_pipe_destroy(inst);
	return false;
}

void relay_pipe_destroy(struct relay_pipe *inst)
{
	/* Notify thread of closing */
	uint64_t n = 1;
	write(inst->fd_end, &n, sizeof(n));
	/* Wait for thread to close */
	pthread_join(inst->piper, NULL);
	/* Clean up */
	relay_client_destroy(&inst->reader);
	relay_client_destroy(&inst->writer);
}
