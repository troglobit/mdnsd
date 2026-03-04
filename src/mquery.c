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

#include "config.h"
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
#include <libmdnsd/sdtxt.h>
#include "config.h"
#include "mcsock.h"


static mdns_daemon_t *d;
static int simple;
static int devmode;
static int terminate_mode;
static char *detail;	/* hostname filter for -d */

struct instance {
	char iname[256];	/* e.g. "MySwitch._http._tcp.local." */
	char hostname[256];	/* from SRV, e.g. "myswitch.local." */
	int  port;		/* from SRV */
	char product[64];	/* from TXT product= */
	char version[64];	/* from TXT version= */
	struct instance *next;
};

struct device {
	char hostname[256];
	char addrs[8][INET6_ADDRSTRLEN];
	int  naddrs;
	struct device *next;
};

static struct instance *instances;
static struct device   *devices;

static struct instance *find_or_create_instance(const char *name)
{
	struct instance *inst;

	for (inst = instances; inst; inst = inst->next)
		if (!strcmp(inst->iname, name))
			return inst;

	inst = calloc(1, sizeof(*inst));
	if (inst) {
		strlcpy(inst->iname, name, sizeof(inst->iname));
		inst->next = instances;
		instances = inst;
	}
	return inst;
}

static struct device *find_or_create_device(const char *hostname)
{
	struct device *dev;

	for (dev = devices; dev; dev = dev->next)
		if (!strcmp(dev->hostname, hostname))
			return dev;

	dev = calloc(1, sizeof(*dev));
	if (dev) {
		strlcpy(dev->hostname, hostname, sizeof(dev->hostname));
		dev->next = devices;
		devices = dev;
	}
	return dev;
}

static void device_add_addr(struct device *dev, const char *addr)
{
	int i;

	for (i = 0; i < dev->naddrs; i++)
		if (!strcmp(dev->addrs[i], addr))
			return;
	if (dev->naddrs < (int)(sizeof(dev->addrs) / sizeof(dev->addrs[0])))
		strlcpy(dev->addrs[dev->naddrs++], addr, INET6_ADDRSTRLEN);
}


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

		rc = sscanf(buf, "%16s %X %X %d %d %d %d %X %d %d %d\n",
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

static int ans_dev(mdns_answer_t *a, void *arg)
{
	char ipstr[INET6_ADDRSTRLEN];
	struct instance *inst;
	struct device *dev;
	xht_t *txt;
	char *val;

	if (a->ttl == 0)
		return 0;

	switch (a->type) {
	case QTYPE_PTR:
		if (!a->rdname || !a->rdname[0])
			break;
		if (!arg)
			/* DISCO_NAME PTR → service types */
			mdnsd_query(d, a->rdname, QTYPE_PTR, ans_dev, a->rdname);
		else {
			/* service type PTR → instance names */
			find_or_create_instance(a->rdname);
			mdnsd_query(d, a->rdname, QTYPE_SRV, ans_dev, NULL);
			mdnsd_query(d, a->rdname, QTYPE_TXT, ans_dev, NULL);
		}
		break;

	case QTYPE_SRV:
		if (!a->rdname || !a->rdname[0])
			break;
		inst = find_or_create_instance(a->name);
		if (!inst->hostname[0]) {
			strlcpy(inst->hostname, a->rdname, sizeof(inst->hostname));
			inst->port = a->srv.port;
			mdnsd_query(d, a->rdname, QTYPE_A,    ans_dev, NULL);
			mdnsd_query(d, a->rdname, QTYPE_AAAA, ans_dev, NULL);
		}
		break;

	case QTYPE_TXT:
		if (!a->rdlen || !a->rdata)
			break;
		inst = find_or_create_instance(a->name);
		txt  = txt2sd(a->rdata, a->rdlen);
		if (txt) {
			val = xht_get(txt, "product");
			if (val && !inst->product[0])
				strlcpy(inst->product, val, sizeof(inst->product));
			val = xht_get(txt, "version");
			if (val && !inst->version[0])
				strlcpy(inst->version, val, sizeof(inst->version));
			xht_free(txt);
		}
		break;

	case QTYPE_A:
		inet_ntop(AF_INET, &a->ip, ipstr, sizeof(ipstr));
		dev = find_or_create_device(a->name);
		device_add_addr(dev, ipstr);
		break;

	case QTYPE_AAAA:
		inet_ntop(AF_INET6, &a->ip6, ipstr, sizeof(ipstr));
		dev = find_or_create_device(a->name);
		device_add_addr(dev, ipstr);
		break;

	default:
		break;
	}

	return 0;
}

/* Strip trailing dot: "myswitch.local." → "myswitch.local" */
static char *shortname(const char *hostname, char *buf, size_t len)
{
	size_t n;

	strlcpy(buf, hostname, len);
	n = strlen(buf);
	if (n > 0 && buf[n - 1] == '.')
		buf[n - 1] = 0;
	return buf;
}

static int matches_detail(struct device *dev)
{
	char fqdn[256], bare[256], *p;

	if (!detail)
		return 1;

	shortname(dev->hostname, fqdn, sizeof(fqdn));	/* "myswitch.local"  */
	strlcpy(bare, fqdn, sizeof(bare));
	p = strchr(bare, '.');
	if (p)
		*p = 0;					/* "myswitch"        */

	return !strcmp(detail, fqdn)			/* myswitch.local    */
	    || !strcmp(detail, dev->hostname)		/* myswitch.local.   */
	    || !strcmp(detail, bare);			/* myswitch          */
}

static void print_devices(void)
{
	struct device *dev;
	struct instance *inst;
	int i, name_w = 4, addr_w = 7, prod_w = 7;

	/* First pass: compute column widths from actual data */
	for (dev = devices; dev; dev = dev->next) {
		char name[256];
		int n;

		if (!matches_detail(dev))
			continue;

		shortname(dev->hostname, name, sizeof(name));
		n = (int)strlen(name);
		if (n > name_w)
			name_w = n;

		for (i = 0; i < dev->naddrs; i++) {
			n = (int)strlen(dev->addrs[i]);
			if (n > addr_w)
				addr_w = n;
		}

		for (inst = instances; inst; inst = inst->next) {
			if (strcmp(inst->hostname, dev->hostname) || !inst->product[0])
				continue;
			n = (int)strlen(inst->product);
			if (n > prod_w)
				prod_w = n;
		}
	}
	name_w += 2;
	addr_w += 2;

	/* Inverse-video header */
	printf("\033[7m%-*s %-*s %-*s\033[0m\n",
	       name_w, "NAME", addr_w, "ADDRESS", prod_w, "PRODUCT");

	/* Data rows: one address per line, continuation lines indent only */
	for (dev = devices; dev; dev = dev->next) {
		char name[256], product[64] = "";

		if (!matches_detail(dev))
			continue;

		shortname(dev->hostname, name, sizeof(name));

		for (inst = instances; inst; inst = inst->next) {
			if (!strcmp(inst->hostname, dev->hostname) && inst->product[0]) {
				strlcpy(product, inst->product, sizeof(product));
				break;
			}
		}

		/* First line: name, first address (or blank), product */
		printf("%-*s %-*s %s\n",
		       name_w, name,
		       addr_w, dev->naddrs ? dev->addrs[0] : "",
		       product);

		/* Continuation lines: blank name field, remaining addresses */
		for (i = 1; i < dev->naddrs; i++)
			printf("%-*s %s\n", name_w, "", dev->addrs[i]);
	}
}

static void print_device_detail(void)
{
	struct device *dev;
	struct instance *inst;
	int i, first = 1;

	for (dev = devices; dev; dev = dev->next) {
		if (!matches_detail(dev))
			continue;

		if (!first)
			printf("\n");
		first = 0;

		char fqdn[256];
		printf("Device:  %s\n", shortname(dev->hostname, fqdn, sizeof(fqdn)));
		for (i = 0; i < dev->naddrs; i++)
			printf("Address: %s\n", dev->addrs[i]);

		printf("Services:\n");
		for (inst = instances; inst; inst = inst->next) {
			char svc[256];
			char *p;

			if (strcmp(inst->hostname, dev->hostname))
				continue;

			/* "MySwitch._http._tcp.local." → "_http._tcp" */
			p = strchr(inst->iname, '.');
			strlcpy(svc, p ? p + 1 : inst->iname, sizeof(svc));
			/* strip domain: find dot after _proto label */
			p = strchr(svc, '.');
			if (p) {
				p = strchr(p + 1, '.');
				if (p)
					*p = 0;
			}

			printf("  %-24s port %-6d", svc, inst->port);
			if (inst->product[0])
				printf(" product=%s", inst->product);
			if (inst->version[0])
				printf(" version=%s", inst->version);
			printf("\n");
		}
	}
}

/* Create multicast 224.0.0.251:5353 socket */
static int msock(char *ifname)
{
	struct ifnfo ifa = { 0 };

	if (ifname) {
		memcpy(ifa.ifname, ifname, sizeof(ifa.ifname));
		ifa.ifindex = if_nametoindex(ifname);
		return mdns_socket(&ifa, 0);
	}

	return mdns_socket(NULL, 0);
}


static int usage(int code)
{
	/* mquery -D -T    mquery -t 12 _http._tcp.local. */
	printf("usage: mquery [-hDsTv] "
#ifdef HAVE_SO_BINDTODEVICE
	       "[-i IFNAME] "
#endif
	       "[-d HOST] [-l LEVEL] [-t TYPE] [-w SEC] [NAME]\n");
	return code;
}

int main(int argc, char *argv[])
{
	struct message m;
	struct in_addr ip;
	unsigned short port;
	ssize_t bsize;
	socklen_t ssize;
	unsigned char buf[MAX_PACKET_LEN];
	char default_iface[IFNAMSIZ] = { 0 };
	struct sockaddr_in from, to;
	const char *name = DISCO_NAME;
	char *ifname = NULL;
	int type = QTYPE_PTR;	/* 12 */
	time_t start;
	int wait = 0;
	fd_set fds;
	int sd, c;

	while ((c = getopt(argc, argv, "h?DT"
#ifdef HAVE_SO_BINDTODEVICE
			   "i:"
#endif
			   "d:l:st:vw:")) != EOF) {
		switch (c) {
		case 'h':
		case '?':
			return usage(0);

		case 'D':
			devmode = 1;
			break;

		case 'T':
			terminate_mode = 1;
			break;

		case 'd':
			detail  = optarg;
			devmode = 1;
			break;

#ifdef HAVE_SO_BINDTODEVICE
		case 'i':
			ifname = optarg;
			break;
#endif

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

	start = time(NULL);
	if (devmode) {
		if (!terminate_mode)
			printf("Scanning for devices ... press Ctrl-C to stop\n");
		mdnsd_query(d, DISCO_NAME, QTYPE_PTR, ans_dev, NULL);
	} else {
		printf("Querying for %s type %d ... press Ctrl-C to stop\n", name, type);
		mdnsd_query(d, name, type, ans, NULL);
	}

	time_t last_rx = 0;
	while (1) {
		struct timeval one_sec = { 1, 0 };
		struct timeval *tv = mdnsd_sleep(d);

		/* When terminating, cap select() so we check the quiet timeout often */
		if (terminate_mode && (!tv || tv->tv_sec >= 1))
			tv = &one_sec;

		FD_ZERO(&fds);
		FD_SET(sd, &fds);
		select(sd + 1, &fds, 0, 0, tv);

		if (FD_ISSET(sd, &fds)) {
			ssize = sizeof(struct sockaddr_in);
			while ((bsize = recvfrom(sd, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)&from, &ssize)) > 0) {
				last_rx = time(NULL);
				memset(&m, 0, sizeof(struct message));
				if (message_parse(&m, buf) == 0)
					mdnsd_in(d, &m, from.sin_addr, from.sin_port);
			}
			if (bsize < 0 && errno != EAGAIN) {
				printf("Failed reading from socket %d: %s\n", errno, strerror(errno));
				return 1;
			}
		}

		while (mdnsd_out(d, &m, &ip, &port)) {
			int len = message_packet_len(&m);

			memset(&to, 0, sizeof(to));
			to.sin_family = AF_INET;
			to.sin_port = port;
			to.sin_addr = ip;
			if (sendto(sd, message_packet(&m), len, 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in)) != len) {
				printf("Failed writing to socket: %s\n", strerror(errno));
				return 1;
			}
		}

		if (terminate_mode && last_rx && (time(NULL) - last_rx >= 2))
			break;
		if (wait && (time(NULL) - start >= wait))
			break;
	}

	mdnsd_shutdown(d);
	mdnsd_free(d);

	if (devmode) {
		if (detail)
			print_device_detail();
		else
			print_devices();

		while (instances) {
			struct instance *next = instances->next;
			free(instances);
			instances = next;
		}
		while (devices) {
			struct device *next = devices->next;
			free(devices);
			devices = next;
		}
	}

	return 0;
}
