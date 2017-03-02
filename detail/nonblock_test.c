#if 0
(
set -eu
declare -r tmp="$(mktemp)"
trap "rm -f -- '$tmp'" EXIT ERR
gcc -std=gnu11 -Ic_modules -DSIMPLE_LOGGING -DRELAY_NONBLOCK_TEST -O3 -g -o "$tmp" $(find -name \*.c) -lpthread
# node ./server & declare -ri spid=$!
# trap "kill $spid; wait" EXIT ERR
valgrind -q --track-origins=yes --leak-check=full "$tmp"
)
exit 0
#endif
#if defined RELAY_NONBLOCK_TEST
#include <ctcp/socket.h>
#include "../relay_client.h"

static char buf[6UL << 20];

int main(int argc, char *argv[])
{
	struct socket_client sc;
	struct relay_client rc;
	struct fstr addr;
	struct fstr port;
	fstr_init_ref(&addr, "::1");
	fstr_init_ref(&port, "49501");
	if (!socket_client_init(&sc, &addr, &port, NULL)) {
		log_error("connect failed");
		return 1;
	}
	fcntl(sc.fd, F_SETFL, fcntl(sc.fd, F_GETFL) | O_NONBLOCK);
	if (!relay_client_init_fd(&rc, "nb", sc.fd, false, true) != 0) {
		log_error("init failed");
		return 2;
	}
	fprintf(stderr, "..........\r");
	for (int i = 0; i < 10; i++) {
		if (!relay_client_send_packet(&rc, "big", "nb", buf, sizeof(buf))) {
			log_error("send failed");
			return 3;
		}
		fprintf(stderr, "x");
	}
	printf("\n");
	relay_client_destroy(&rc);
	socket_client_destroy(&sc);
	return 0;
}
#endif
