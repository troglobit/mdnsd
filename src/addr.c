/*
 * Copyright (c) 2018-2022  Joachim Wiberg <troglobit@gmail.com>
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

#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "mdnsd.h"

static TAILQ_HEAD(iflist, iface) iface_list = TAILQ_HEAD_INITIALIZER(iface_list);

struct iface *iface_iterator(int first)
{
	static struct iface *next = NULL;
	struct iface *iface;

	if (first)
		iface = TAILQ_FIRST(&iface_list);
	else
		iface = next;

	/* prepare for next, in case iface is unlinked */
	if (iface)
		next = TAILQ_NEXT(iface, link);

	return iface;
}

struct iface *iface_find(const char *ifname)
{
	struct iface *iface;

	for (iface = iface_iterator(1); iface; iface = iface_iterator(0)) {
		if (strcmp(iface->ifname, ifname))
			continue;

		return iface;
	}

	return NULL;
}

void iface_free(struct iface *iface)
{
	if (!iface)
		return;

	TAILQ_REMOVE(&iface_list, iface, link);
	free(iface);
}

static void mark(void)
{
	struct iface *iface;

	for (iface = iface_iterator(1); iface; iface = iface_iterator(0)) {
		iface->unused = 1;
		iface->inaddr_old = iface->inaddr;
		memset(&iface->inaddr, 0, sizeof(iface->inaddr));
		iface->in6addr_old = iface->in6addr;
		memset(&iface->in6addr, 0, sizeof(iface->in6addr));
	}
}

static int sweep(void)
{
	struct iface *iface;
	int changed = 0;

	for (iface = iface_iterator(1); iface; iface = iface_iterator(0)) {
		if (iface->unused)
			continue;

		if (!iface->changed)
			continue;

		if (iface->inaddr.s_addr == iface->inaddr_old.s_addr &&	IN6_ARE_ADDR_EQUAL(&iface->in6addr, &iface->in6addr_old)) {
			iface->changed = 0;
			continue;
		}

		changed++;
	}

	return changed;
}

void iface_init(char *ifname)
{
	struct ifaddrs *ifaddr, *ifa;
	struct iface *iface = NULL;
	int rc = -1;

	rc = getifaddrs(&ifaddr);
	if (rc) {
		ERR("Failed fetching system interfaces: %s", strerror(errno));
		return;
	}

	mark();

	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		struct in6_addr ina;
		char buf[INET6_ADDRSTRLEN];


		if (!ifa->ifa_addr)
			continue;

		if (ifa->ifa_flags & IFF_LOOPBACK)
			continue; /* skip for now, mDNSResponder has it as fallback */

		if (!(ifa->ifa_flags & IFF_MULTICAST))
			continue;

		if (!(ifa->ifa_flags & IFF_UP))
			continue; /* skip for now, not enabled */

		if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		if (ifname && strcmp(ifname, ifa->ifa_name))
			continue;

		/* Validate IP address */
		socklen_t salen = ifa->ifa_addr->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
		rc = getnameinfo(ifa->ifa_addr, salen, buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
		if (rc)
			continue;

		/* Use address from getnameinfo() */
		char* pc = strchr(buf, '%');
		if (pc != NULL)  /* inet_pton cannot handle the interface part of a link local IPv6 address. */
			*pc = '\0';
		if (inet_pton(ifa->ifa_addr->sa_family, buf, &ina) <= 0)
			continue;

		if (!iface)
			iface = iface_find(ifa->ifa_name);

		if (!iface) {
			iface = calloc(1, sizeof(*iface));
			if (!iface) {
				ERR("Failed allocating memory for iface %s: %s", ifa->ifa_name, strerror(errno));
				exit(1);
			}

			DBG("Creating iface instance for interface %s, address %s", ifa->ifa_name, buf);
			TAILQ_INSERT_TAIL(&iface_list, iface, link);

			strlcpy(iface->ifname, ifa->ifa_name, sizeof(iface->ifname));
			iface->ifindex = if_nametoindex(ifa->ifa_name);
			iface->hostid = 1;
			iface->sd = -1;
		} else {
			iface->unused = 0;
		}

		if (ifa->ifa_addr->sa_family == AF_INET) {
			if (iface->inaddr.s_addr != ina.s6_addr32[0]) {
				if (is_zeronet(&iface->inaddr) || is_linklocal(&iface->inaddr)) {
					iface->inaddr.s_addr = ina.s6_addr32[0];
					iface->changed = 1;
				}
			}
		} else {
			if (! IN6_ARE_ADDR_EQUAL(&iface->in6addr, &ina)) {
				if (IN6_IS_ADDR_UNSPECIFIED(&iface->in6addr)    /* Everything is better than no address (::) */
					/* Any address is preferred over link local address */
					|| IN6_IS_ADDR_LINKLOCAL(&iface->in6addr)
					/* Any address is preferred over a site local address except for a link local address. */
					|| (IN6_IS_ADDR_SITELOCAL(&iface->in6addr) && !IN6_IS_ADDR_LINKLOCAL(&ina))
					/* A unique local address shall only be overwritten by a global address. */
					|| ((iface->in6addr.s6_addr[0] & 0xfe) == 0xfc && !IN6_IS_ADDR_SITELOCAL(&ina) && !IN6_IS_ADDR_LINKLOCAL(&ina))
					) {
					iface->in6addr = ina;
					iface->changed = 1;
				}
			}
		}

		/* prepare for next */
		if (!ifname)
			iface = NULL;
	}
	freeifaddrs(ifaddr);

	sweep();
}

void iface_exit(void)
{
	struct iface *iface, *tmp;

	TAILQ_FOREACH_SAFE(iface, &iface_list, link, tmp) {
		TAILQ_REMOVE(&iface_list, iface, link);
		free(iface);
	}
}
