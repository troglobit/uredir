/* UDP service redirector
 *
 * Copyright (c) 2017       Tobias Waldekranz <tobias@waldekranz.com>
 * Copyright (C) 2017-2018  Joachim Nilsson <troglobit@gmail.com>
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

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/udp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <uev/uev.h>
#include "uredir.h"

#define _d(_fmt, args...)					\
	syslog(LOG_DEBUG, "dbg %-15s " _fmt, __func__, ##args)

#define _e(_fmt, args...)					\
	syslog(LOG_ERR, "ERR %-15s " _fmt, __func__, ##args)

#define conn_dump(c)						\
	_d("remote:%s:%u", inet_ntoa(c->remote->sin_addr),	\
	   ntohs(c->remote->sin_port));				\
	_d("local:%s sd:%d", inet_ntoa(c->local), c->sd);


#define CBUFSIZ 512

static uev_t outer_watcher;
static struct sockaddr_in outer;
static struct sockaddr_in inner;

struct conn {
	LIST_ENTRY(conn) list;

	int    sd;
	uev_t  watcher;
	uev_t  timer;

	struct msghdr *hdr;
	struct in_addr local;
	struct sockaddr_in *remote;
};

LIST_HEAD(connhead, conn) conns;
#define conn_foreach(_c) LIST_FOREACH(_c, &conns, list)

static void conn_del(struct conn *c);


struct msghdr *hdr_new(void)
{
	struct msghdr *hdr;

	hdr = malloc(sizeof(*hdr));
	assert(hdr);

	hdr->msg_name = malloc(sizeof(struct sockaddr_in));
	hdr->msg_namelen = sizeof(struct sockaddr_in);
	assert(hdr->msg_name);

	hdr->msg_control = malloc(CBUFSIZ);
	hdr->msg_controllen = CBUFSIZ;
	assert(hdr->msg_control);

	hdr->msg_iov = malloc(sizeof(struct iovec));
	hdr->msg_iovlen = 1;
	assert(hdr->msg_iov);

	hdr->msg_iov->iov_base = malloc(BUFSIZ);
	hdr->msg_iov->iov_len = BUFSIZ;
	assert(hdr->msg_iov->iov_base);

	return hdr;
}

void hdr_free(struct msghdr *hdr)
{
	free(hdr->msg_iov->iov_base);
	free(hdr->msg_iov);
	free(hdr->msg_control);
	free(hdr->msg_name);
	free(hdr);
}

static void timer_cb(uev_t *w, void *arg, int events)
{
	if (events & UEV_ERROR)
		return;

	if (!inetd) {
		_d("Connection timeout, cleaning up.");
		conn_del((struct conn *)arg);
		return;
	}

	_d("Timeout, exiting.");
	uev_exit(w->ctx);
}

void timer_reset(struct conn *c)
{
	_d("");
	uev_timer_set(&c->timer, timeout * 1000, 0);
}

/* Peek into socket to figure out where an inbound packet comes from */
static struct in_addr *peek(int sd, void *name, socklen_t len)
{
	static char cmbuf[0x100];
	struct msghdr msgh;
	struct cmsghdr *cmsg;

	memset(&msgh, 0, sizeof(msgh));
	msgh.msg_name = name;
	msgh.msg_namelen = len;
	msgh.msg_control = cmbuf;
	msgh.msg_controllen = sizeof(cmbuf);

	if (recvmsg(sd, &msgh, MSG_PEEK) < 0)
		return NULL;

	for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg; cmsg = CMSG_NXTHDR(&msgh, cmsg)) {
		struct in_pktinfo *ipi = (struct in_pktinfo *)CMSG_DATA(cmsg);

		if (cmsg->cmsg_level != SOL_IP || cmsg->cmsg_type != IP_PKTINFO)
			continue;

		return &ipi->ipi_spec_dst;
	}

	return NULL;
}

int sock_new(int *sock)
{
	int sd = *sock;
	int opt = 0, on = 1;

	if (sd < 0) {
		sd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sd < 0) {
			syslog(LOG_ERR, "Failed opening UDP socket: %m");
			return -1;
		}
	}

	/* Socket must be non-blocking for libev/libuEv */
	fcntl(sd, F_SETFL, fcntl(sd, F_GETFL) | O_NONBLOCK);

	/* At least on Linux the obnoxious IP_MULTICAST_ALL flag is set by default */
	if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_ALL, &opt, sizeof(opt)))
		goto error;

	if (setsockopt(sd, SOL_IP, IP_PKTINFO, &on, sizeof(on)))
		goto error;

	*sock = sd;
	return 0;

error:
	close(sd);
	return -1;
}

static void conn_to_outer(uev_t *w, void *arg, int events)
{
	struct conn *c = (struct conn *)arg;
	ssize_t n;

	_d("");
	if (events & UEV_ERROR)
		return;
	conn_dump(c);

	n = recv(c->sd, c->hdr->msg_iov->iov_base, BUFSIZ, 0);
	if (n <= 0) {
		_e("recv:%d\n", errno);
		conn_del(c);
		return;
	}

	timer_reset(c);
	c->hdr->msg_iov->iov_len = n;
	sendto(outer_watcher.fd, c->hdr->msg_iov->iov_base, n, 0, c->hdr->msg_name, c->hdr->msg_namelen);
}

static struct conn *conn_find(struct in_addr *local, struct sockaddr_in *remote)
{
	struct conn *c;

	conn_foreach(c) {
		if (c->remote->sin_addr.s_addr == remote->sin_addr.s_addr &&
		    c->remote->sin_port        == remote->sin_port        &&
		    c->local.s_addr            == local->s_addr) {
			_d("found\n");
			return c;
		}
	}
	_d("missing\n");

	return NULL;
}

static struct conn *conn_new(uev_ctx_t *ctx, struct in_addr *local, struct sockaddr_in *remote)
{
	struct msghdr *hdr;
	struct conn *c;

	hdr = hdr_new();
	if (!hdr)
		return NULL;

	if (hdr->msg_namelen > sizeof(*remote))
		hdr->msg_namelen = sizeof(*remote);
	memcpy(hdr->msg_name, remote, hdr->msg_namelen);

	c = malloc(sizeof(*c));
	if (!c) {
		hdr_free(hdr);
		return NULL;
	}

	c->sd = -1;
	c->hdr = hdr;
	c->remote = hdr->msg_name;
	c->local = *local;

	if (sock_new(&c->sd)) {
		free(c);
		hdr_free(hdr);
		return NULL;
	}

	if (connect(c->sd, (struct sockaddr *)&inner, sizeof(inner))) {
		close(c->sd);
		free(c);
		hdr_free(hdr);
		return NULL;
	}

	LIST_INSERT_HEAD(&conns, c, list);

	uev_timer_init(ctx, &c->timer, timer_cb, c, timeout * 1000, 0);
	uev_io_init(ctx, &c->watcher, conn_to_outer, c, c->sd, UEV_READ);

	_d("");
	conn_dump(c);

	return c;
}

static void conn_del(struct conn *c)
{
	uev_io_stop(&c->watcher);
	close(c->watcher.fd);

	uev_timer_stop(&c->timer);

	hdr_free(c->hdr);
	LIST_REMOVE(c, list);
	free(c);
}

static void outer_to_inner(uev_t *w, void *arg, int events)
{
	struct sockaddr_in sin;
	struct in_addr *local;
	struct conn *c;
	ssize_t len;

	_d("\n");
	if (events & UEV_ERROR)
		return;

	local = peek(w->fd, &sin, sizeof(sin));
	if (!local) {
		_e("Failed peeking into message: %m");
		uev_exit(w->ctx);
	}

	c = conn_find(local, &sin);
	if (!c) {
		c = conn_new(w->ctx, local, &sin);
		if (!c) {
			_e("Failed allocating new connection: %m");
			return;
		}
	}

	len = recvmsg(w->fd, c->hdr, 0);
	if (len == -1) {
		_e("Failed receiving message: %m");
		return;
	}

	timer_reset(c);

	_d("");
	conn_dump(c);

	send(c->sd, c->hdr->msg_iov->iov_base, len, 0);
}

static int outer_init(char *addr, short port)
{
	int sd = -1;

	if (sock_new(&sd))
		return -1;

	memset(&outer, 0, sizeof(outer));
	outer.sin_family = AF_INET;
	inet_aton(addr, &outer.sin_addr);
	outer.sin_port = htons(port);
	if (bind(sd, (struct sockaddr *)&outer, sizeof(outer))) {
		syslog(LOG_ERR, "Failed binding our address (%s:%d): %m", addr, port);
		return -1;
	}

	_d("ready\n");

	return sd;
}

int redirect_init(uev_ctx_t *ctx, char *src, short src_port, char *dst, short dst_port)
{
	int sd;

	memset(&inner, 0, sizeof(inner));
	inner.sin_family = AF_INET;
	inet_aton(dst, &inner.sin_addr);
	inner.sin_port = htons(dst_port);

	if (!src) {
		/* Running as an inetd service */
		sd = STDIN_FILENO;
		if (sock_new(&sd))
			return 1;
	} else {
		sd = outer_init(src, src_port);
		if (sd < 0)
			return 1;
	}

	return uev_io_init(ctx, &outer_watcher, outer_to_inner, NULL, sd, UEV_READ);
}

int redirect_exit(void)
{
	struct conn *c;

	while (!LIST_EMPTY(&conns)) {
		c = LIST_FIRST(&conns);
		conn_del(c);
	}

	return uev_io_stop(&outer_watcher) || close(outer_watcher.fd);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
