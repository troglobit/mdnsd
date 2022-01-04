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

#ifndef MDNSD_H_
#define MDNSD_H_

#include "config.h"

#include <net/if.h>		/* IFNAMSIZ */
#include <libmdnsd/mdnsd.h>
#include <libmdnsd/sdtxt.h>

#include "queue.h"

/* From The Practice of Programming, by Kernighan and Pike */
#ifndef NELEMS
#define NELEMS(array) (sizeof(array) / sizeof((array)[0]))
#endif

#ifndef IN_ZERONET
#define IN_ZERONET(addr) ((addr & IN_CLASSA_NET) == 0)
#endif

#ifndef IN_LOOPBACK
#define IN_LOOPBACK(addr) ((addr & IN_CLASSA_NET) == 0x7f000000)
#endif

#ifndef IN_LINKLOCAL
#define IN_LINKLOCALNETNUM 0xa9fe0000
#define IN_LINKLOCAL(addr) ((addr & IN_CLASSB_NET) == IN_LINKLOCALNETNUM)
#endif

struct iface {
	TAILQ_ENTRY(iface) link;
	char               unused;
	char               changed;

	char               ifname[IFNAMSIZ];
	int                ifindex;		/* Physical interface index   */
	struct in_addr     inaddr;		/* == 0 for non IP interfaces */
	struct in_addr     inaddr_old;
	int                sd;

	mdns_daemon_t     *mdns;
};

void mdnsd_conflict(char *name, int type, void *arg);

/* addr.c */
struct iface *iface_iterator(int first);
struct iface *iface_find(const char *ifname);
void          iface_free(struct iface *iface);
int           iface_update(char *ifname);

void          iface_init(char *ifname);
void          iface_exit(void);

/* conf.c */
int conf_init(mdns_daemon_t *d, char *path, int hostid);

/* replacement functions for systems that don't have them  */
#ifndef HAVE_PIDFILE
int pidfile(const char *basename);
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

static inline int is_linklocal(struct in_addr *ina)
{
	return IN_LINKLOCAL(ntohl(ina->s_addr));
}

static inline int is_zeronet(struct in_addr *ina)
{
	return IN_ZERONET(ntohl(ina->s_addr));
}

static inline int is_add_valid(struct in_addr *ina)
{
	in_addr_t addr;

	addr = ntohl(ina->s_addr);
	if (IN_ZERONET(addr) || IN_LOOPBACK(addr) || IN_LINKLOCAL(addr))
		return 0;

	return 1;
}

#endif /* MDNSD_H_ */
