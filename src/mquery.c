/*
 * Copyright (c) 2003  Jeremie Miller <jer@jabber.org>
 * Copyright (c) 2016-2022  Joachim Wiberg <troglobit@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libmdnsd/mdnsd.h>
#include "config.h"
#include "mcsock.h"


char *prognm = "mquery";
mdns_daemon_t *d;
int simple;


#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

/* Find default outbound *LAN* interface, i.e. skipping tunnels */
static char *getifname(char *ifname, size_t len)
{
	uint32_t dest, gw, mask;
	char buf[256], name[17];
	FILE *fp;
	int found = 0;

	fp = fopen("/proc/net/route", "r");
	if (!fp)
		return NULL;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		int rc, flags, cnt, use, metric, mtu, win, irtt;

		rc = sscanf(buf, "%16s %X %X %X %d %d %d %X %d %d %d\n",
			   name, &dest, &gw, &flags, &cnt, &use, &metric,
			   &mask, &mtu, &win, &irtt);

		if (rc < 10 || !(flags & 1)) /* IFF_UP */
			continue;

		if (dest != 0 || mask != 0)
			continue;

		if (!ifname[0] || !strncmp(ifname, "tun", 3)) {
			strlcpy(ifname, name, len);
			found = 1;
			break;
		}
	}
	fclose(fp);

	if (found)
		return ifname;

	return NULL;
}

static const char *type2str(int type)
{
	static char str[20] = { 0 };

	switch(type) {
	case QTYPE_A:
		return "A (1)";

	case QTYPE_NS:
		return "NS (2)";

	case QTYPE_CNAME:
		return "CNAME (5)";

	case QTYPE_PTR:
		return "TR (12)";

	case QTYPE_TXT:
		return "TXT (16)";

	case QTYPE_AAAA:
		return "AAAA (28)";

	case QTYPE_SRV:
		return "SRV (33)";

	case QTYPE_ANY:
		return "ANY (255)";

	default:
		snprintf(str, sizeof(str), "UNKNOWN (%d)", type);
		break;
	}

	return str;
}

/* Print an answer */
static int ans(mdns_answer_t *a, void *arg)
{
	int now;
	char ipinput[INET6_ADDRSTRLEN];

	if (a->ttl == 0)
		now = 0;
	else
		now = a->ttl - time(0);

	if (!simple) {
		char *spec = (char *)arg;

		if (a->type != QTYPE_PTR)
			return 0;

		if (!spec)
			mdnsd_query(d, a->rdname, a->type, ans, a->rdname);

		printf("+ %s (%s)\n", a->rdname, inet_ntoa(a->ip));
		return 0;
	}

	switch (a->type) {
	case QTYPE_A:
		inet_ntop(AF_INET, &(a->ip), ipinput, INET_ADDRSTRLEN);
		printf("A %s for %d seconds to ip %s\n", a->name, now, ipinput);
		break;

	case QTYPE_AAAA:
		inet_ntop(AF_INET6, &(a->ip6), ipinput, INET6_ADDRSTRLEN);
		printf("AAAA %s for %d seconds to ip %s\n", a->name, now, ipinput);
		break;

	case QTYPE_PTR:
		printf("PTR %s for %d seconds to %s\n", a->name, now, a->rdname);
		break;

	case QTYPE_SRV:
		printf("SRV %s for %d seconds to %s:%d\n", a->name, now, a->rdname, a->srv.port);
		break;

	default:
		printf("%s %s for %d seconds with %d data\n", type2str(a->type), a->name, now, a->rdlen);
	}

	return 0;
}

/* Create multicast 224.0.0.251:5353 socket */
static int msock(char *ifname)
{
	struct ifnfo ifa = { 0 };

	if (ifname) {
		memcpy(ifa.ifname, ifname, sizeof(ifa.ifname));
		ifa.ifindex = if_nametoindex(ifname);
		return mdns_ipv4_socket(&ifa, 0);
	}

	return mdns_ipv4_socket(NULL, 0);
}


static int usage(int code)
{
	/* mquery -t 12 _http._tcp.local. */
	printf("usage: mquery [-hsv] [-i IFNAME] [-l LEVEL] [-t TYPE] [-w SEC] [NAME]\n");
	return code;
}

int main(int argc, char *argv[])
{
	struct message m;
	ssize_t bsize;
	socklen_t ssize;
	unsigned char buf[MAX_PACKET_LEN];
	char default_iface[IFNAMSIZ] = { 0 };
	struct sockaddr_storage from, to;
	char *name = DISCO_NAME;
	char *ifname = NULL;
	int type = QTYPE_PTR;	/* 12 */
	time_t start;
	int wait = 0;
	fd_set fds;
	int sd, c;

	while ((c = getopt(argc, argv, "h?i:l:st:vw:")) != EOF) {
		switch (c) {
		case 'h':
		case '?':
			return usage(0);

		case 'i':
			ifname = optarg;
			break;

		case 'l':
			if (-1 == mdnsd_log_level(optarg))
				return usage(1);
			break;

		case 's':
			simple = 1;
			break;

		case 't':
			type = atoi(optarg);
			break;

		case 'v':
			puts(PACKAGE_VERSION);
			return 0;

		case 'w':
			wait = atoi(optarg);
			break;

		default:
			return usage(1);
		}
	}

	if (optind < argc)
		name = argv[optind];

	if (!ifname)
		ifname = getifname(default_iface, sizeof(default_iface));

	sd = msock(ifname);
	if (sd == -1) {
		printf("Failed creating multicast socket: %s\n", strerror(errno));
		return 1;
	}

	d = mdnsd_new(1, 1000);
	if (!d)
		return 1;

	printf("Querying for %s type %d ... press Ctrl-C to stop\n", name, type);
	start = time(NULL);
	mdnsd_query(d, name, type, ans, NULL);

	while (1) {
		struct timeval *tv = mdnsd_sleep(d);

		FD_ZERO(&fds);
		FD_SET(sd, &fds);
		select(sd + 1, &fds, 0, 0, tv);

		if (FD_ISSET(sd, &fds)) {
			ssize = sizeof(from);
			while ((bsize = recvfrom(sd, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)&from, &ssize)) > 0) {
				memset(&m, 0, sizeof(struct message));
				if (message_parse(&m, buf) == 0)
					mdnsd_in(d, &m, &from);
			}
			if (bsize < 0 && errno != EAGAIN) {
				printf("Failed reading from socket %d: %s\n", errno, strerror(errno));
				return 1;
			}
		}

		memset(&to, 0, sizeof(to));
		to.ss_family = AF_INET;
		while (mdnsd_out(d, &m, &to) > 0) {
			int len = message_packet_len(&m);

			if (sendto(sd, message_packet(&m), len, 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in)) != len) {
				printf("Failed writing to socket: %s\n", strerror(errno));
				return 1;
			}
			memset(&to, 0, sizeof(to));
			to.ss_family = AF_INET;
		}

		if (wait && (time(NULL) - start >= wait))
			break;
	}

	mdnsd_shutdown(d);
	mdnsd_free(d);

	return 0;
}
