#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <ev.h>
#include <linux/udp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define _d(_fmt, ...)						\
	fprintf(stderr, "dbg %-15s " _fmt, __func__,		\
		##__VA_ARGS__)

#define _e(_fmt, ...)						\
	fprintf(stderr, "ERR %-15s " _fmt, __func__,		\
		##__VA_ARGS__)


#define CBUFSIZ 512

static ev_io outer_watcher;
static struct sockaddr_in inner;

struct conn {
	ev_io watcher;
	LIST_ENTRY(conn) list;

	int sd;

	struct msghdr *hdr;
	struct in_addr *local;
	struct sockaddr_in *remote;
};

LIST_HEAD(connhead, conn) conns;

struct msghdr *hdr_new(void) {
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

void hdr_extract_da(struct msghdr *hdr, struct in_addr **da)
{
	struct in_pktinfo *pktinfo;
	struct cmsghdr *cmsg;

	if (!hdr->msg_controllen) {
		*da = NULL;
		return;
	}

	for (cmsg = CMSG_FIRSTHDR(hdr); cmsg; cmsg = CMSG_NXTHDR(hdr, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IP &&
		    cmsg->cmsg_type  == IP_PKTINFO) {
			pktinfo = (struct in_pktinfo *)CMSG_DATA(cmsg);
			*da = &pktinfo->ipi_spec_dst;
			return;
		}
	}

	*da = NULL;
	return;
}

#define conn_foreach(_c) for((_c) = conns.lh_first; (_c); (_c) = (_c)->list.le_next)

static void conn_dump(struct conn *c, FILE *fp)
{
	fprintf(fp, "remote:%s:%u ",
		inet_ntoa(c->remote->sin_addr), ntohs(c->remote->sin_port));

	fprintf(fp, "local:%s sd:%d\n", inet_ntoa(*(c->local)), c->sd);
}

static void conn_to_outer(EV_P_ ev_io *w, int revents)
{
	struct conn *c = (void *)w;
	ssize_t n;

	(void)(revents);

	_d(""); conn_dump(c, stderr);

	n = recv(c->sd, c->hdr->msg_iov->iov_base, BUFSIZ, 0);
	if (n <= 0) {
		_e("recv:%d\n", errno);
		return;
	}

	c->hdr->msg_iov->iov_len = n;
	sendmsg(outer_watcher.fd, c->hdr, 0);
}

static void conn_to_inner(struct conn *c, struct msghdr *hdr)
{
	_d(""); conn_dump(c, stderr);

	send(c->sd, hdr->msg_iov->iov_base, hdr->msg_iov->iov_len, 0);
}

static struct conn *conn_find(struct msghdr *hdr)
{
	struct sockaddr_in *remote = hdr->msg_name;
	struct in_addr *local;
	struct conn *c;

	hdr_extract_da(hdr, &local);

	conn_foreach(c) {
		if (c->remote->sin_addr.s_addr == remote->sin_addr.s_addr &&
		    c->remote->sin_port == remote->sin_port &&
		    c->local->s_addr == local->s_addr) {
			_d("found\n");
			return c;
		}
	}

	_d("missing\n");
	return NULL;
}

static struct conn *conn_new(struct msghdr *hdr)
{
	struct conn *c;

	c = malloc(sizeof(*c));
	assert(c);

	c->hdr = hdr;
	c->remote = hdr->msg_name;
	hdr_extract_da(hdr, &c->local);

	c->sd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (c->sd < 0)
		return NULL;

	if (connect(c->sd, (struct sockaddr *)&inner, sizeof(inner)))
		return NULL;

	LIST_INSERT_HEAD(&conns, c, list);

	ev_io_init(&c->watcher, conn_to_outer, c->sd, EV_READ);
	ev_io_start (EV_DEFAULT, &c->watcher);

	_d(""); conn_dump(c, stderr);
	return c;
}

static void outer_to_inner(EV_P_ ev_io *w, int revents)
{
	struct msghdr *hdr;
	struct conn *c;
	int res;

	_d("\n");

	hdr = hdr_new();

	res = recvmsg(w->fd, hdr, 0);
	if (res == -1)
		exit(1);

	c = conn_find(hdr);
	if (c) {
		conn_to_inner(c, hdr);
		hdr_free(hdr);
	} else {
		c = conn_new(hdr);
		if (!c) {
			hdr_free(hdr);
			return;
		}

		conn_to_inner(c, hdr);
	}
}

static int outer_init(struct sockaddr_in *addr)
{
	int sd, on = 1;

	sd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (sd < 0)
		return -1;

	if (setsockopt(sd, SOL_IP, IP_PKTINFO, &on, sizeof(on)))
		return -1;

	if (bind(sd, (struct sockaddr *)addr, sizeof(*addr)))
		return -1;

	_d("ready\n");
	ev_io_init(&outer_watcher, outer_to_inner, sd, EV_READ);
	ev_io_start (EV_DEFAULT, &outer_watcher);
	return 0;
}

int main (void)
{
	struct sockaddr_in addr = { 0 };
	int err;

	inner.sin_family = AF_INET;
	inet_aton("127.0.0.1", &inner.sin_addr);
	inner.sin_port = htons(40404);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(4040);
	err = outer_init(&addr);
	if (err)
		exit(1);

	ev_run (EV_DEFAULT, 0);
	return 0;
}
