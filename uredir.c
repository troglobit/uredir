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
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int echo       = 0;
static int inetd      = 0;
static int background = 1;
static int do_syslog  = 1;
extern char *__progname;

static int loglvl(char *level)
{
	int i;

	for (i = 0; prioritynames[i].c_name; i++) {
		if (!strcmp(prioritynames[i].c_name, level))
			return prioritynames[i].c_val;
	}

	return atoi(level);
}

static int version(void)
{
	printf("%s\n", PACKAGE_VERSION);
	return 0;
}

#define USAGE "Usage: %s [-hinsv] [-l LEVEL] [SRC:PORT] [DST:PORT]"

static int usage(int code)
{
	if (inetd) {
		syslog(LOG_ERR, USAGE, __progname);
		return code;
	}

	printf("\n" USAGE "\n\n", __progname);
	printf("  -h      Show this help text\n");
	printf("  -i      Run in inetd mode, get SRC:PORT from stdin\n");
	printf("  -l LVL  Set log level: none, err, info, notice (default), debug\n");
	printf("  -n      Run in foreground, do not detach from controlling terminal\n");
	printf("  -s      Use syslog, even if running in foreground, default w/o -n\n");
	printf("  -v      Show program version\n\n");
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

static void exit_cb(int signo)
{
	syslog(LOG_DEBUG, "Got signal %d, exiting.", signo);
	exit(0);
}

/*
 * read from in, forward to out, creating a socket pipe ... or tube
 *
 * If no @dst is given then we're in echo mode, send everything back
 * If no @src is given then we should forward the reply
 */
static int tuby(int sd, struct sockaddr_in *src, struct sockaddr_in *dst)
{
	int n;
	char buf[BUFSIZ], addr[50];
	struct sockaddr_in sa;
	static struct sockaddr_in da;
	socklen_t sn = sizeof(sa);

	syslog(LOG_DEBUG, "Reading %s socket ...", src ? "client" : "proxy");
	n = recvfrom(sd, buf, sizeof(buf), 0, (struct sockaddr *)&sa, &sn);
	if (n <= 0) {
		if (n < 0)
			syslog(LOG_ERR, "Failed receiving data: %m");
		return 0;
	}

	syslog(LOG_DEBUG, "Received %d bytes data from %s:%d", n, inet_ntop(AF_INET, &sa.sin_addr, addr, sn), ntohs(sa.sin_port));

	/* Echo mode, return everything to sender */
	if (!dst)
		return sendto(sd, buf, n, 0, (struct sockaddr *)&sa, sn);

	/* Verify the received packet is the actual reply before we forward it */
	if (!src) {
		if (sa.sin_addr.s_addr != da.sin_addr.s_addr || sa.sin_port != da.sin_port)
			return 0;
	} else {
		*src = sa;	/* Tell callee who called */
		da   = *dst;	/* Remember for forward of reply. */
	}

	syslog(LOG_DEBUG, "Forwarding %d bytes data to %s:%d", n, inet_ntop(AF_INET, &dst->sin_addr, addr, sizeof(*dst)), ntohs(dst->sin_port));
	n = sendto(sd, buf, n, 0, (struct sockaddr *)dst, sizeof(*dst));
	if (n <= 0) {
		if (n < 0)
			syslog(LOG_ERR, "Failed forwarding data: %m");
		return 0;
	}

	return n;
}

int main(int argc, char *argv[])
{
	int c, sd, src_port, dst_port;
	int opt = 0;
	int log_opts = LOG_CONS | LOG_PID;
	int loglevel = LOG_NOTICE;
	char src[20], dst[20];
	struct sockaddr_in sa;
	struct sockaddr_in da;

	while ((c = getopt(argc, argv, "hil:nsv")) != EOF) {
		switch (c) {
		case 'h':
			return usage(0);

		case 'i':
			inetd = 1;
			break;

		case 'l':
			loglevel = loglvl(optarg);
			if (-1 == loglevel)
				return usage(1);
			break;

		case 'n':
			background = 0;
			do_syslog--;
			break;

		case 's':
			do_syslog++;
			break;

		case 'v':
			return version();

		default:
			return usage(-1);
		}
	}

	if (!background && do_syslog < 1)
		log_opts |= LOG_PERROR;
	openlog(__progname, log_opts, LOG_DAEMON);
	setlogmask(LOG_UPTO(loglevel));

	if (optind >= argc)
		return usage(-2);

	signal(SIGALRM, exit_cb);
	signal(SIGHUP,  exit_cb);
	signal(SIGINT,  exit_cb);
	signal(SIGQUIT, exit_cb);
	signal(SIGTERM, exit_cb);

	if (inetd) {
		/* In inetd mode we redirect from src=stdin to dst:port */
		dst_port = parse_ipport(argv[optind], dst, sizeof(dst));
	} else {
		/* By default we need at least src:port */
		src_port = parse_ipport(argv[optind++], src, sizeof(src));
		if (-1 == src_port)
			return usage(-3);

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
		sd = STDIN_FILENO;
	} else {
		sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		if (sd < 0) {
			syslog(LOG_ERR, "Failed opening UDP socket: %m");
			return 1;
		}

		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = inet_addr(src);
		sa.sin_port = htons(src_port);
		if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
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

	/* At least on Linux the obnoxious IP_MULTICAST_ALL flag is set by default */
	setsockopt(sd, IPPROTO_IP, IP_MULTICAST_ALL, &opt, sizeof(opt));

	while (1) {
		if (inetd)
			alarm(3);

		if (echo)
			tuby(sd, NULL, NULL);
		else if (tuby(sd, &sa, &da))
			tuby(sd, NULL, &sa);
	}

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
