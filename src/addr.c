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

int iface_update(char *ifname)
{
	struct ifaddrs *ifaddr, *ifa;
	struct iface *iface = NULL;
	int changed = 0;
	int rc = -1;

	rc = getifaddrs(&ifaddr);
	if (rc) {
		ERR("Failed fetching system interfaces: %s", strerror(errno));
		return 0;
	}

	if (ifname)
		iface = iface_find(ifname);

	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		struct in_addr ina;
		char buf[20];

		if (!ifa->ifa_addr)
			continue;

		if (ifa->ifa_flags & IFF_LOOPBACK)
			continue; /* skip for now, mDNSResponder has it as fallback */

		if (!(ifa->ifa_flags & IFF_MULTICAST))
			continue;

		if (!(ifa->ifa_flags & IFF_UP))
			continue; /* skip for now, not enabled */

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		if (ifname && strcmp(ifname, ifa->ifa_name))
			continue;

		/* Validate IP address */
		rc = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
		if (rc)
			continue;

		/* Use address from getnameinfo() */
		if (!inet_aton(buf, &ina))
			continue;

		if (!ifname)
			iface = iface_find(ifa->ifa_name);

		if (!iface) {
			iface = calloc(1, sizeof(*iface));
			if (!iface) {
				ERR("Failed allocating memory for iface %s: %s", ifa->ifa_name, strerror(errno));
				exit(1);
			}
			TAILQ_INSERT_TAIL(&iface_list, iface, link);

			strlcpy(iface->ifname, ifa->ifa_name, sizeof(iface->ifname));
			iface->ifindex = if_nametoindex(ifa->ifa_name);
			iface->sd = -1;
		} else {
			iface->unused = 0;
		}

		if (iface->inaddr.s_addr != ina.s_addr) {
			DBG("Found interface %s, address %s", ifa->ifa_name, buf);
			iface->inaddr = ina;
			iface->changed = 1;
			changed++;
		}
		iface = NULL;

		if (ifname)
			break;
	}
	freeifaddrs(ifaddr);

	return changed;
}

void iface_init(char *ifname)
{
	iface_update(ifname);
}

void iface_exit(void)
{
	struct iface *iface, *tmp;

	TAILQ_FOREACH_SAFE(iface, &iface_list, link, tmp) {
		TAILQ_REMOVE(&iface_list, iface, link);
		free(iface);
	}
}
