#if 0
(
set -eu
declare -r tmp="$(mktemp)"
trap "rm -f -- '$tmp'" EXIT ERR
gcc -std=gnu11 -I../c_modules -DSIMPLE_LOGGING -DRELAY_NONBLOCK_TEST -O3 -g -o "$tmp" $(find ../ -name \*.c) -lpthread
# node ./server & declare -ri spid=$!
# trap "kill $spid; wait" EXIT ERR
valgrind -q --track-origins=yes --leak-check=full "$tmp"
)
exit 0
#endif
#if defined RELAY_NONBLOCK_TEST
#include <cstd/unix.h>
#include <ctcp/socket.h>
#include "../relay_client.h"

static char buf[262144];

int main(int argc, char *argv[])
{
	struct socket_client sc;
	struct relay_client rc;
	struct fstr addr;
	struct fstr port;
	fstr_init_ref(&addr, "::1");
	fstr_init_ref(&port, "49501");
	pid_t f = fork();
	const char *me = f ? "red" : "blue";
	const char *you = f ? "blue" : "red";
	if (!socket_client_init(&sc, &addr, &port, NULL)) {
		log_error("connect failed");
		return 1;
	}
	if (fcntl(sc.fd, F_SETFL, fcntl(sc.fd, F_GETFL) | O_NONBLOCK) == -1) {
		log_error("fcntl failed");
		return 5;
	}
	if (!relay_client_init_fd(&rc, me, sc.fd, false, true) != 0) {
		log_error("init failed");
		return 2;
	}
	for (int i = 0; i < sizeof(buf); i++) {
		buf[i] = i & 0xff;
	}
	if (f) {
		fprintf(stderr, "..........\r");
	}
	for (int i = 0; i < 10; i++) {
		if (!relay_client_send_packet(&rc, "big", you, buf, sizeof(buf))) {
			log_error("send failed");
			return 3;
		}
		if (f) {
			fprintf(stderr, "-");
		}
		struct relay_packet *p;
		if (!relay_client_recv_packet(&rc, &p) || p == NULL) {
			log_error("Failed to receive packet");
			return 5;
		}
		if (p->length != sizeof(buf) || memcmp(p->data, buf, sizeof(buf)) != 0) {
			log_error("incorrect data received");
			return 4;
		}
		free(p);
		if (f) {
			fprintf(stderr, "\x1b[1Dx");
		}
	}
	if (f) {
		printf("\n");
	}
	relay_client_destroy(&rc);
	socket_client_destroy(&sc);
	if (f) {
		wait(NULL);
	}
	return 0;
}
#endif
