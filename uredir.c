/*
 * Copyright (C) 2007-2008  Ivan Tikhonov <kefeer@brokestream.com>
 * Copyright (C) 2016  Joachim Nilsson <troglobit@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int echo       = 0;
static int inetd      = 0;
static int background = 1;
extern char *__progname;

static int version(void)
{
	printf("%s\n", PACKAGE_VERSION);
	return 0;
}

static int usage(int code)
{
	printf("\nUsage: %s [-hinv] [SRC:PORT] [DST:PORT]\n\n", __progname);

	printf("  -h  Show this help text\n");
	printf("  -i  Run in inetd mode, get SRC:PORT from stdin\n");
	printf("  -n  Run in foreground, do not detach from controlling terminal\n");
	printf("  -v  Show program version\n\n");
	printf("If DST:PORT is left out the program operates in echo mode.\n"
	       "Bug report address: %-40s\n\n", PACKAGE_BUGREPORT);

	return code;
}

static int parse_ipport(char *arg, char *buf, size_t len)
{
	char *ptr;

	if (!arg || !buf || !len)
		return -1;

	ptr = strchr(arg, ':');
	if (!ptr)
		return -1;

	*ptr++ = 0;
	snprintf(buf, len, "%s", arg);

	return atoi(ptr);
}

static int open_udp_socket(void)
{
	int sd;

	sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sd < 0) {
		syslog(LOG_ERR, "Failed opening UDP socket: %m");
		exit(1);
	}

	return sd;
}

/*
 * read from in, forward to out, creating a socket pipe ... or tube
 *
 * If no @dst is given then we're in echo mode, send everything back
 * If no @src is given then we should forward the reply
 */
static int tuby(int in, int out, struct sockaddr_in *src, struct sockaddr_in *dst)
{
	int n;
	char buf[BUFSIZ], addr[50];
	struct sockaddr_in sa;
	static struct sockaddr_in da;
	socklen_t sn = sizeof(sa);

	syslog(LOG_DEBUG, "Reading %s socket ...", src ? "client" : "proxy");
	n = recvfrom(in, buf, sizeof(buf), 0, (struct sockaddr *)&sa, &sn);
	if (n <= 0) {
		if (n < 0)
			syslog(LOG_ERR, "Failed receiving data: %m");
		return 0;
	}

	syslog(LOG_DEBUG, "Received %d bytes data from %s:%d", n, inet_ntop(AF_INET, &sa.sin_addr, addr, sn), ntohs(sa.sin_port));

	/* Echo mode, return everything to sender */
	if (!dst)
		return sendto(in, buf, n, 0, (struct sockaddr *)&sa, sn);

	/* Verify the received packet is the actual reply before we forward it */
	if (!src) {
		if (sa.sin_addr.s_addr != da.sin_addr.s_addr || sa.sin_port != da.sin_port)
			return 0;
	} else {
		*src = sa;	/* Tell callee who called */
		da   = *dst;	/* Remember for forward of reply. */
	}

	syslog(LOG_DEBUG, "Forwarding %d bytes data to %s:%d", n, inet_ntop(AF_INET, &dst->sin_addr, addr, sizeof(*dst)), ntohs(dst->sin_port));
	n = sendto(out, buf, n, 0, (struct sockaddr *)dst, sizeof(*dst));
	if (n <= 0) {
		if (n < 0)
			syslog(LOG_ERR, "Failed forwarding data: %m");
		return 0;
	}

	return n;
}

int main(int argc, char *argv[])
{
	int c, in, out, src_port, dst_port;
	char src[20], dst[20];
	struct sockaddr_in sa;
	struct sockaddr_in da;

	while ((c = getopt(argc, argv, "hinv")) != EOF) {
		switch (c) {
		case 'h':
			return usage(0);

		case 'i':
			inetd = 1;
			break;

		case 'n':
			background = 0;
			break;

		case 'v':
			return version();

		default:
			return usage(1);
		}
	}

	if (optind >= argc)
		return usage(1);

	if (inetd) {
		/* In inetd mode we redirect from src=stdin to dst:port */
		dst_port = parse_ipport(argv[optind], dst, sizeof(dst));
	} else {
		/* By default we need at least src:port */
		src_port = parse_ipport(argv[optind++], src, sizeof(src));
		if (-1 == src_port)
			return usage(1);

		dst_port = parse_ipport(argv[optind], dst, sizeof(dst));
	}

	/* If no dst, then user wants to echo everything back to src */
	if (-1 == dst_port) {
		echo = 1;
	} else {
		da.sin_family = AF_INET;
		da.sin_addr.s_addr = inet_addr(dst);
		da.sin_port = htons(dst_port);
	}

	if (inetd) {
		in = STDIN_FILENO;
		if (echo)
			out = in;
		else
			out = open_udp_socket();
	} else {
		in = out = open_udp_socket();

		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = inet_addr(src);
		sa.sin_port = htons(src_port);
		if (bind(in, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
			syslog(LOG_ERR, "Failed binding our address (%s:%d): %m", src, src_port);
			return 1;
		}

		if (background) {
			if (-1 == daemon(0, 0)) {
				syslog(LOG_ERR, "Failed daemonizing: %m");
				return 2;
			}
		}
	}

	while (1) {
		if (echo)
			tuby(in, in, NULL, NULL);
		else if (tuby(in, out, &sa, &da))
			tuby(out, in, NULL, &sa);

		if (inetd)
			break;
	}

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
