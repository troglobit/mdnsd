/*
 * Copyright (c) 2022  Florian Zschocke
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
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "config.h"
#include "libmdnsd/mdnsd.h"
#include "mcsock.h"



static struct in_addr get_addr(const char *ifname)
{
	struct ifaddrs *ifaddr, *ifa;
	struct in_addr addr;
	addr.s_addr = htonl(INADDR_ANY);

	if (getifaddrs(&ifaddr) < 0) {
		WARN("Failed getting interfaces: %s", strerror(errno));
		return addr;
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET &&
			strcmp(ifname, ifa->ifa_name) == 0) {
			addr = ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
			break;
		}
	}

	freeifaddrs(ifaddr);
	return addr;
}



static int mc_socket_mcast_join_ipv4(int sd, struct ifnfo *iface, struct sockaddr_in *sin)
{
#ifdef HAVE_STRUCT_IP_MREQN_IMR_IFINDEX
	struct ip_mreqn imr ={ 0 };
#else
	struct ip_mreq imr = { 0 };
#endif


	if (iface) {
		if (iface->ifindex != 0 && iface->inaddr.s_addr == 0) {
			iface->inaddr = get_addr(iface->ifname);
		}

		/* Set interface for outbound multicast */
#ifdef HAVE_STRUCT_IP_MREQN_IMR_IFINDEX
		imr.imr_ifindex = iface->ifindex;
		imr.imr_address = iface->inaddr;
		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &imr, sizeof(imr)))
			WARN("Failed setting IP_MULTICAST_IF to %d: %s", iface->ifindex, strerror(errno));
#else
		imr.imr_interface = iface->inaddr;
		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, &(iface->inaddr), sizeof(struct in_addr)))
			WARN("Failed setting IP_MULTICAST_IF to %s: %s", inet_ntoa(iface->inaddr), strerror(errno));
#endif
	}


	/* Join link-local multicast group on the given interface. */
	imr.imr_multiaddr = sin->sin_addr;
	if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(imr)))
		WARN("Failed joining IPv4 multicast group %s: %s", inet_ntoa(imr.imr_multiaddr), strerror(errno));
	else
		INFO("Joining mDNS IPv4 multicast group %s%s", iface?" on iface " : "", iface?iface->ifname:"");

	return sd;
}


static int mc_socket_mcast_join_ipv6(int sd, struct ifnfo *iface, struct sockaddr_in6 *sin6)
{
	struct ipv6_mreq i6mr = { 0 };


	if (iface) {
		/* Set interface for outbound multicast */
		if (setsockopt(sd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &(iface->ifindex), sizeof(iface->ifindex)))
			WARN("Failed setting IPV6_MULTICAST_IF to %d: %s", iface->ifindex, strerror(errno));

		i6mr.ipv6mr_interface = iface->ifindex;
	}

	/* Join link-local multicast group on the given interface. */
	i6mr.ipv6mr_multiaddr = sin6->sin6_addr;
	if (setsockopt(sd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &i6mr, sizeof(i6mr)))
		WARN("Failed joining IPv6 multicast group: %s", strerror(errno));
	else
		INFO("Joining mDNS IPv6 multicast group %s%s", iface?" on iface " : "", iface?iface->ifname:"");

	return sd;
}


static int mc_socket_setup_ipv4(int sd, struct ifnfo *iface, struct sockaddr_in *sin)
{
	struct sockaddr_in local_addr = { 0 };

	if (iface) {
		/* Filter inbound traffic from anyone (ANY) to port 5353 on ifname */
		if (setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, &iface->ifname, strlen(iface->ifname)))
			INFO("Failed setting SO_BINDTODEVICE on %s: %s", iface->ifname, strerror(errno));
	}


	local_addr.sin_family = AF_INET;
	local_addr.sin_port = sin->sin_port;
	if (bind(sd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in))) {
		close(sd);
		ERR("Failed binding socket to *:%d: %s", ntohs(local_addr.sin_port), strerror(errno));
		return -1;
	}
	INFO("Bound to *:%d%s%s", ntohs(local_addr.sin_port), iface?" on iface " : "", iface?iface->ifname:"");


	mc_socket_mcast_join_ipv4(sd, iface, sin);

	return sd;
}


static int mc_socket_setup_ipv6(int sd, struct ifnfo *iface, struct sockaddr_in6 *sin6)
{
	const int on = 1;
	struct sockaddr_in6 local_addr = { 0 };


	if (iface) {
		/* Filter inbound traffic from anyone (ANY) to port 5353 on ifname */
		if (setsockopt(sd, SOL_SOCKET, SO_BINDTODEVICE, &iface->ifname, strlen(iface->ifname)))
			INFO("Failed setting SO_BINDTODEVICE on %s: %s", iface->ifname, strerror(errno));
	}

	if (setsockopt(sd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)))
		WARN("Failed setting IPV6_V6ONLY: %s", strerror(errno));



	local_addr.sin6_family = AF_INET6;
	local_addr.sin6_port = sin6->sin6_port;
	if (bind(sd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in6))) {
		close(sd);
		ERR("Failed binding socket to :: :%d: %s", ntohs(local_addr.sin6_port), strerror(errno));
		return -1;
	}
	INFO("Bound to *:%d%s%s", ntohs(local_addr.sin6_port), iface?" on iface " : "", iface?iface->ifname:"");


	mc_socket_mcast_join_ipv6(sd, iface, sin6);

	return sd;
}


static int mc_socket(struct ifnfo *iface, unsigned char ttl, struct sockaddr *saddr)
{
	const int on = 1;
	const int off = 0;

	const bool ipv6 = (saddr->sa_family == AF_INET6);
	const int so_level = (saddr->sa_family == AF_INET6) ? IPPROTO_IPV6 : IPPROTO_IP;

	int so_optname;
	socklen_t len;
	int unicast_ttl = 255;
	int multicast_ttl = ttl;
	int bufsiz;
	int sd;



	sd = socket(saddr->sa_family, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (sd < 0) {
		ERR("Failed creating UDP socket: %s", strerror(errno));
		return -1;
	}

	/* Set IP independent socket options */
#ifdef SO_REUSEPORT
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)))
		WARN("Failed setting SO_REUSEPORT: %s", strerror(errno));
#endif
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
		WARN("Failed setting SO_REUSEADDR: %s", strerror(errno));

	/* Double the size of the receive buffer (getsockopt() returns the double) */
	len = sizeof(bufsiz);
	if (!getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &bufsiz, &len)) {
		if (setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &bufsiz, sizeof(bufsiz)))
			INFO("Failed doubling the size of the receive buffer: %s", strerror(errno));
	}



	/* Set socket options on IP level */
	so_optname = (ipv6) ? IPV6_MULTICAST_LOOP : IP_MULTICAST_LOOP;
	if (setsockopt(sd, so_level, so_optname, &on, sizeof(on)))
		WARN("Failed enabling IP%s_MULTICAST_LOOP on %s: %s", (ipv6)?"V6":"", iface->ifname, strerror(errno));

	if (! ipv6) {
		if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_ALL, &off, sizeof(off)))
			WARN("Failed disabling IP_MULTICAST_ALL on %s: %s", iface->ifname, strerror(errno));
	}

	/*
	 * All traffic on mDNS is link-local only, so the default
	 * TTL is set to 1.  Some users may however want to route mDNS.
	 */
	so_optname = (ipv6) ? IPV6_MULTICAST_HOPS : IP_MULTICAST_TTL;
	if (setsockopt(sd, so_level, so_optname, &multicast_ttl, sizeof(multicast_ttl)))
		WARN("Failed setting %s to %d: %s", (ipv6)?"IPV6_MULTICAST_HOPS":"IP_MULTICAST_TTL", multicast_ttl, strerror(errno));

	/* mDNS also supports unicast, so we need a relevant TTL there too */
	so_optname = (ipv6) ? IPV6_UNICAST_HOPS : IP_TTL;
	if (setsockopt(sd, so_level, so_optname, &unicast_ttl, sizeof(unicast_ttl)))
		WARN("Failed setting %s to %d: %s", (ipv6)?"IPV6_UNICAST_HOPS":"IP_TTL", unicast_ttl, strerror(errno));



	if (saddr->sa_family == AF_INET6)
		return mc_socket_setup_ipv6(sd, iface, (struct sockaddr_in6*)saddr);

	return mc_socket_setup_ipv4(sd, iface, (struct sockaddr_in*)saddr);
}



int mdns_ipv4_socket(struct ifnfo *iface, unsigned char ttl)
{
	/* Default to TTL of 1 for mDNS as it is link-local */
	if (ttl == 0)
		ttl = 1;

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(5353);
	inet_pton(AF_INET, "224.0.0.251", &(sin.sin_addr));

	return mc_socket(iface, ttl, (struct sockaddr*)&sin);
}


int mdns_ipv6_socket(struct ifnfo *iface, unsigned char ttl)
{
	/* Default to TTL of 1 for mDNS as it is link-local */
	if (ttl == 0)
		ttl = 1;

	struct sockaddr_in6 sin6 = { 0 };
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(5353);
	inet_pton(AF_INET6, "ff02::fb", &(sin6.sin6_addr));

	return mc_socket(iface, ttl, (struct sockaddr*)&sin6);
}
