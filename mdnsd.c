#include <config.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <libmdnsd/mdnsd.h>
#include <libmdnsd/sdtxt.h>

volatile sig_atomic_t running = 1;

char *prognm  = PACKAGE_NAME;
mdns_daemon_t *_d;

static void conflict(char *name, int type, void *arg)
{
	printf("conflicting name detected %s for type %d\n", name, type);
	exit(1);
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
	mdns_daemon_t *d;
	mdns_record_t *r;
	struct message m;
	struct in_addr ip;
	unsigned short int port;
	struct timeval *tv;
	ssize_t bsize;
	socklen_t ssize;
	unsigned char buf[MAX_PACKET_LEN];
	struct sockaddr_in from, to;
	fd_set fds;
	int s;
	char hlocal[256], nlocal[256];
	unsigned char *packet;
	int len = 0;
	xht_t *h;
	char *path = NULL;

	prognm = progname(argv[0]);
	if (argc < 4) {
		printf("usage: %s 'unique name' 12.34.56.78 80 '/optionalpath'\n", prognm);
		return 1;
	}

	inet_aton(argv[2], &ip);
	port = atoi(argv[3]);
	if (argc == 5)
		path = argv[4];
	printf("Announcing .local site named '%s' to %s:%d and extra path '%s'\n", argv[1], inet_ntoa(ip), port, argv[4]);

	signal(SIGINT, done);
	signal(SIGHUP, done);
	signal(SIGQUIT, done);
	signal(SIGTERM, done);
	_d = d = mdnsd_new(QCLASS_IN, 1000);
	if ((s = msock()) == 0) {
		printf("can't create socket: %s\n", strerror(errno));
		return 1;
	}

	sprintf(hlocal, "%s._http._tcp.local.", argv[1]);
	sprintf(nlocal, "%s.local.", argv[1]);
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

	while (running) {
		tv = mdnsd_sleep(d);
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		select(s + 1, &fds, 0, 0, tv);

		if (FD_ISSET(s, &fds)) {
			ssize = sizeof(struct sockaddr_in);
			while ((bsize = recvfrom(s, buf, MAX_PACKET_LEN, 0, (struct sockaddr *)&from, &ssize)) > 0) {
				memset(&m, 0, sizeof(struct message));
				message_parse(&m, buf);
				mdnsd_in(d, &m, (unsigned long int)from.sin_addr.s_addr, from.sin_port);
			}
			if (bsize < 0 && errno != EAGAIN) {
				printf("can't read from socket %d: %s\n", errno, strerror(errno));
				return 1;
			}
		}

		while (mdnsd_out(d, &m, (long unsigned int *)&ip, &port)) {
			memset(&to, 0, sizeof(to));
			to.sin_family = AF_INET;
			to.sin_port = port;
			to.sin_addr = ip;
			if (sendto(s, message_packet(&m), message_packet_len(&m), 0, (struct sockaddr *)&to,
				   sizeof(struct sockaddr_in)) != message_packet_len(&m)) {
				printf("can't write to socket: %s\n", strerror(errno));
				return 1;
			}

			printf(">> %s", inet_ntoa(from.sin_addr));
		}
	}

	mdnsd_shutdown(d);
	mdnsd_free(d);

	return 0;
}
