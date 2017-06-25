/*
 * Copyright (C) 2016-2017  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"
#include <ev.h>
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

static int inetd      = 0;
static int background = 1;
static int do_syslog  = 1;
static char *prognm   = PACKAGE_NAME;

int redirect(char *src, short src_port, char *dst, short dst_port);


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

#define USAGE "Usage: %s [-hinsv] [-I NAME] [-l LEVEL] [SRC:PORT] DST:PORT"

static int usage(int code)
{
	if (inetd) {
		syslog(LOG_ERR, USAGE, prognm);
		return code;
	}

	printf("\n" USAGE "\n\n", prognm);
	printf("  -h      Show this help text\n");
	printf("  -i      Run in inetd mode, get SRC:PORT from stdin\n");
	printf("  -I NAME Identity, tag syslog messages with NAME, default: process name\n");
	printf("  -l LVL  Set log level: none, err, info, notice (default), debug\n");
	printf("  -n      Run in foreground, do not detach from controlling terminal\n");
	printf("  -s      Use syslog, even if running in foreground, default w/o -n\n");
	printf("  -v      Show program version\n\n");
	printf("Bug report address: %-40s\n\n", PACKAGE_BUGREPORT);

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

static void exit_cb(struct ev_loop *loop, ev_signal *w, int revents)
{
	syslog(LOG_DEBUG, "Got signal %d, exiting.", w->signum);
	ev_unloop(loop, EVUNLOOP_ALL);
}

static void timer_cb (struct ev_loop *loop, ev_timer *w, int revents)
{
	ev_unloop(loop, EVUNLOOP_ALL);
}

static char *progname(char *arg0)
{
	char *nm;

	nm = strrchr(arg0, '/');
	if (nm)
		nm++;
	else
		nm = arg0;

	return nm;
}

int main(int argc, char *argv[])
{
	int c, src_port = 0, dst_port = 0;
	int log_opts = LOG_CONS | LOG_PID;
	int loglevel = LOG_NOTICE;
	char *ident;
	char src[20], dst[20];
	ev_signal signal_watcher;

	ident = prognm = progname(argv[0]);
	while ((c = getopt(argc, argv, "hiI:l:nsv")) != EOF) {
		switch (c) {
		case 'h':
			return usage(0);

		case 'i':
			inetd = 1;
			break;

		case 'I':
			ident = strdup(optarg);
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
	openlog(ident, log_opts, LOG_DAEMON);
	setlogmask(LOG_UPTO(loglevel));

	if (optind >= argc)
		return usage(-2);

	ev_signal_init(&signal_watcher, exit_cb, SIGALRM);
	ev_signal_init(&signal_watcher, exit_cb, SIGHUP);
	ev_signal_init(&signal_watcher, exit_cb, SIGINT);
	ev_signal_init(&signal_watcher, exit_cb, SIGQUIT);
	ev_signal_init(&signal_watcher, exit_cb, SIGTERM);
	ev_signal_start(EV_DEFAULT, &signal_watcher);

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
		fprintf(stderr, "Missing DST:PORT, aborting.\n");
		return 1;
	}

	if (background) {
		if (-1 == daemon(0, 0)) {
			syslog(LOG_ERR, "Failed daemonizing: %m");
			return 2;
		}
	}

	if (inetd) {
		ev_timer timeout;

		ev_timer_init(&timeout, timer_cb, 3.0, 0.0);
		ev_timer_start(EV_DEFAULT, &timeout);

		if (redirect(NULL, 0, dst, dst_port))
			return 1;
	} else {
		if (redirect(src, src_port, dst, dst_port))
			return 1;
	}

	ev_run(EV_DEFAULT, 0);

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
