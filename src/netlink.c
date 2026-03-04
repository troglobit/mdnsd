/* Netlink interface monitor for Linux
 *
 * Copyright (c) 2026  Joachim Wiberg <troglobit@gmail.com>
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

#ifdef __linux__

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "mdnsd.h"
#include "netlink.h"

#define NL_BUFSZ 4096

/*
 * Open a netlink socket subscribed to interface link and address events.
 * Returns the socket fd on success, -1 on failure.
 */
int netlink_init(void)
{
	struct sockaddr_nl sa = { 0 };
	int sd;

	sd = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (sd < 0) {
		ERR("Failed creating netlink socket: %s", strerror(errno));
		return -1;
	}

	sa.nl_family = AF_NETLINK;
	sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;
	if (bind(sd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		ERR("Failed binding netlink socket: %s", strerror(errno));
		close(sd);
		return -1;
	}

	return sd;
}

/*
 * Drain all pending netlink messages.  Returns 1 if any link or address
 * change event was seen (caller should re-run interface discovery), 0 if
 * nothing relevant arrived, or -1 on a fatal socket error.
 *
 * ENOBUFS means the kernel dropped events because we were too slow; treat
 * that as an implicit change so the caller rescans to catch up.
 */
int netlink_read(int sd)
{
	char buf[NL_BUFSZ];
	struct nlmsghdr *nh;
	ssize_t len;
	size_t l;
	int changed = 0;

	while (1) {
		len = recv(sd, buf, sizeof(buf), 0);
		if (len < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			if (errno == EINTR)
				continue;
			if (errno == ENOBUFS) {
				WARN("Netlink buffer overflow, interface events may have been lost");
				changed = 1;
				break;
			}
			ERR("Netlink recv error: %s", strerror(errno));
			return -1;
		}

		l = (size_t)len;
		for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, l); nh = NLMSG_NEXT(nh, l)) {
			switch (nh->nlmsg_type) {
			case RTM_NEWLINK:
			case RTM_DELLINK:
			case RTM_NEWADDR:
			case RTM_DELADDR:
				changed = 1;
				break;
			default:
				break;
			}
		}
	}

	return changed;
}

void netlink_exit(int sd)
{
	if (sd >= 0)
		close(sd);
}

#endif /* __linux__ */
