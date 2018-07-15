/*
 * Copyright (c) 2003  Jeremie Miller <jer@jabber.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <config.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <libmdnsd/mdnsd.h>
#include <libmdnsd/sdtxt.h>
#include "mdnsd.h"

volatile sig_atomic_t running = 1;

char *prognm  = PACKAGE_NAME;
mdns_daemon_t *_d;

static void conflict(char *name, int type, void *arg)
{
	ERR("conflicting name detected %s for type %d", name, type);
	exit(1);
}

static void record_received(const struct resource* r, void* data)
{
	char ipinput[INET_ADDRSTRLEN];

	switch(r->type) {
	case QTYPE_A:
		inet_ntop(AF_INET, &(r->known.a.ip), ipinput, INET_ADDRSTRLEN);
		DBG("Got %s: A %s->%s", r->name, r->known.a.name, ipinput);
		break;

	case QTYPE_NS:
		DBG("Got %s: NS %s", r->name, r->known.ns.name);
		break;

	case QTYPE_CNAME:
		DBG("Got %s: CNAME %s", r->name, r->known.cname.name);
		break;

	case QTYPE_PTR:
		DBG("Got %s: PTR %s", r->name, r->known.ptr.name);
		break;

	case QTYPE_TXT:
		DBG("Got %s: TXT %s", r->name, r->rdata);
		break;

	case QTYPE_SRV:
		DBG("Got %s: SRV %d %d %d %s", r->name, r->known.srv.priority,
		    r->known.srv.weight, r->known.srv.port, r->known.srv.name);
		break;

	default:
		DBG("Got %s: unknown", r->name);

	}
}

static void done(int sig)
{
	running = 0;
	mdnsd_shutdown(_d);
}

/* Create multicast 224.0.0.251:5353 socket */
static int msock(void)
{
	int s, flag = 1;
	struct sockaddr_in in;
	struct ip_mreq mc;

	memset(&in, 0, sizeof(in));
	in.sin_family = AF_INET;
	in.sin_port = htons(5353);
	in.sin_addr.s_addr = 0;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return 0;

#ifdef SO_REUSEPORT
	setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (char *)&flag, sizeof(flag));
#endif
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));
	if (bind(s, (struct sockaddr *)&in, sizeof(in))) {
		close(s);
		return 0;
	}

	mc.imr_multiaddr.s_addr = inet_addr("224.0.0.251");
	mc.imr_interface.s_addr = htonl(INADDR_ANY);
	setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc, sizeof(mc));

	flag = fcntl(s, F_GETFL, 0);
	flag |= O_NONBLOCK;
	fcntl(s, F_SETFL, flag);

	return s;
}

static int usage(int code)
{
	printf("Usage: %s [-hv] [-n NAME] [-a ADDRESS] [-p PORT] [PATH]\n"
	       "\n"
	       "    -a ADDR   Address of service/host to announce, default: auto\n"
	       "    -h        This help text\n"
	       "    -n NAME   Name of service/host to announce, default: hostname\n"
	       "    -p PORT   Port of service to announce, default: 80\n"
	       "    -v        Show program version\n"
	       "\n"
	       "Bug report address: %-40s\n", prognm, PACKAGE_BUGREPORT);

	return code;
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
	struct timeval tv = { 0 };
	mdns_daemon_t *d;
	mdns_record_t *r;
	struct in_addr ip = { 0 };
	unsigned short int port = 80;
	fd_set fds;
	int c, s;
	char hlocal[256], nlocal[256];
	char hostname[256] = { 0 };
	char address[20];
	unsigned char *packet;
	int len = 0;
	xht_t *h;
	char *path = NULL;

	prognm = progname(argv[0]);
	while ((c = getopt(argc, argv, "a:hn:p:v?")) != EOF) {
		switch (c) {
		case 'a':
			inet_aton(optarg, &ip);
			break;

		case 'h':
		case '?':
			return usage(0);

		case 'n':
			strncpy(hostname, optarg, sizeof(hostname));
			break;

		case 'p':
			port = atoi(optarg);
			break;

		case 'v':
			puts(PACKAGE_VERSION);
			return 0;

		default:
			break;
		}
	}

	if (optind < argc)
		path = argv[optind];

	if (!ip.s_addr) {
		if (!getaddr(address, sizeof(address)))
			errx(1, "Cannot find default interface, use -a ADDRESS");
		inet_aton(address, &ip);
	}

	if (!hostname[0])
		gethostname(hostname, sizeof(hostname));

	INFO("Announcing .local site named '%s' to %s:%d and extra path '%s'",
	     hostname, inet_ntoa(ip), port, path);

	signal(SIGINT, done);
	signal(SIGHUP, done);
	signal(SIGQUIT, done);
	signal(SIGTERM, done);
	_d = d = mdnsd_new(QCLASS_IN, 1000);
	if ((s = msock()) == 0) {
		ERR("Failed creating socket: %s", strerror(errno));
		return 1;
	}

	sprintf(hlocal, "%s._http._tcp.local.", hostname);
	sprintf(nlocal, "%s.local.", hostname);

	mdnsd_register_receive_callback(d, record_received, NULL);


	// Announce that we have a _http._tcp service
	r = mdnsd_shared(d, "_services._dns-sd._udp.local.", QTYPE_PTR, 120);
	mdnsd_set_host(d, r, "_http._tcp.local.");

	r = mdnsd_shared(d, "_http._tcp.local.", QTYPE_PTR, 120);
	mdnsd_set_host(d, r, hlocal);
	r = mdnsd_unique(d, hlocal, QTYPE_SRV, 600, conflict, 0);
	mdnsd_set_srv(d, r, 0, 0, port, nlocal);
	r = mdnsd_unique(d, nlocal, QTYPE_A, 600, conflict, 0);
	mdnsd_set_raw(d, r, (char *)&ip.s_addr, 4);
	r = mdnsd_unique(d, hlocal, QTYPE_TXT, 600, conflict, 0);
	h = xht_new(11);
	if (path && strlen(path))
		xht_set(h, "path", path);
	packet = sd2txt(h, &len);
	xht_free(h);
	mdnsd_set_raw(d, r, (char *)packet, len);
	free(packet);

	/* example: how to read a previously published record */
	r = mdnsd_get_published(d, "_http._tcp.local.");
	while (r) {
		const mdns_answer_t *data;

		data = mdnsd_record_data(r);
		if (data)
			DBG("Found record of type %d", data->type);

		r = mdnsd_record_next(r);
	}

	while (running) {
		int rc;

		FD_ZERO(&fds);
		FD_SET(s, &fds);
		select(s + 1, &fds, NULL, NULL, &tv);

		rc = mdnsd_step(d, s, FD_ISSET(s, &fds), true, &tv);
		if (rc == 1) {
			ERR("Failed reading from socket %d: %s", errno, strerror(errno));
			break;
		}
		if (rc == 2) {
			ERR("Failed writing to socket: %s", strerror(errno));
			break;
		}
	}

	mdnsd_shutdown(d);
	mdnsd_free(d);

	return 0;
}
