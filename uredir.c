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
#include <uev/uev.h>

int inetd   = 0;
int timeout = 3;

static int background = 1;
static int do_syslog  = 1;
static char *ident    = PACKAGE_NAME;
static char *prognm   = PACKAGE_NAME;

int redirect(uev_ctx_t *ctx, char *src, short src_port, char *dst, short dst_port);
int redirect_exit(void);


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

#define USAGE "Usage: %s [-hinsv] [-I NAME] [-l LEVEL] [-t SEC] [SRC:PORT] DST:PORT"

static int usage(int code)
{
	if (inetd) {
		syslog(LOG_ERR, USAGE, prognm);
		return code;
	}

	printf(USAGE "\n\n"
	       "  -h      Show this help text\n"
	       "  -i      Run in inetd mode, get SRC:PORT from stdin, implies -n\n"
	       "  -I NAME Identity, tag syslog messages with NAME, default: %s\n"
	       "  -l LVL  Set log level: none, err, info, notice (default), debug\n"
	       "  -n      Run in foreground, do not detach from controlling terminal\n"
	       "  -s      Use syslog, even if running in foreground, default w/o -n\n"
	       "  -t SEC  Timeout for connections, default 3 seconds\n"
	       "  -v      Show program version\n\n"
	       "Bug report address: %-40s\n\n", prognm, ident, PACKAGE_BUGREPORT);

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

static void exit_cb(uev_t *w, void *arg, int events)
{
	syslog(LOG_DEBUG, "Got signal %d, exiting.", w->signo);
	redirect_exit();
	uev_exit(w->ctx);
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
	char src[20], dst[20];
	uev_t sigalarm_watcher, sighup_watcher, sigint_watcher, sigquit_watcher, sigterm_watcher;
	uev_ctx_t ctx;

	ident = prognm = progname(argv[0]);
	while ((c = getopt(argc, argv, "hiI:l:nst:v")) != EOF) {
		switch (c) {
		case 'h':
			return usage(0);

		case 'i':
			inetd = 1;
			background = 0;
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

		case 't':
			timeout = atoi(optarg);
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

	if (inetd) {
		/* In inetd mode we redirect from src=stdin to dst:port */
		dst_port = parse_ipport(argv[optind], dst, sizeof(dst));
	} else {
		/* By default we need at least src:port */
		src_port = parse_ipport(argv[optind++], src, sizeof(src));
		if (-1 == src_port)
			return usage(-3);

		if (strlen(src) < 7)
			strncpy(src, "0.0.0.0", sizeof(src));

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

	uev_init(&ctx);
	uev_signal_init(&ctx, &sigalarm_watcher, exit_cb, NULL, SIGALRM);
	uev_signal_init(&ctx, &sighup_watcher,   exit_cb, NULL, SIGHUP);
	uev_signal_init(&ctx, &sigint_watcher,   exit_cb, NULL, SIGINT);
	uev_signal_init(&ctx, &sigquit_watcher,  exit_cb, NULL, SIGQUIT);
	uev_signal_init(&ctx, &sigterm_watcher,  exit_cb, NULL, SIGTERM);

	if (inetd) {
		if (redirect(&ctx, NULL, 0, dst, dst_port))
			return 1;
	} else {
		if (redirect(&ctx, src, src_port, dst, dst_port))
			return 1;
	}

	return uev_run(&ctx, UEV_NONE);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
