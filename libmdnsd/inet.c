/* IPv4/IPv6 address helpers, cf. the inet.[ch] wrapper in SMCRoute */
#include "config.h"
#include <arpa/inet.h>
#include <string.h>

#include "inet.h"

sa_family_t inet_family(const inet_addr_t *ss)
{
	return ss->ss_family;
}

socklen_t inet_len(const inet_addr_t *ss)
{
	if (ss->ss_family == AF_INET6)
		return sizeof(struct sockaddr_in6);
	return sizeof(struct sockaddr_in);
}

in_port_t inet_port(const inet_addr_t *ss)
{
#ifdef ENABLE_IPV6
	if (ss->ss_family == AF_INET6)
		return ntohs(((const struct sockaddr_in6 *)ss)->sin6_port);
#endif
	return ntohs(((const struct sockaddr_in *)ss)->sin_port);
}

void inet_set_port(inet_addr_t *ss, in_port_t port)
{
#ifdef ENABLE_IPV6
	if (ss->ss_family == AF_INET6) {
		((struct sockaddr_in6 *)ss)->sin6_port = htons(port);
		return;
	}
#endif
	((struct sockaddr_in *)ss)->sin_port = htons(port);
}

void inet_anyaddr(sa_family_t family, in_port_t port, inet_addr_t *ss)
{
	memset(ss, 0, sizeof(*ss));
	ss->ss_family = family;

#ifdef ENABLE_IPV6
	if (family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;

		sin6->sin6_addr = in6addr_any;
		sin6->sin6_port = htons(port);
		return;
	}
#endif
	((struct sockaddr_in *)ss)->sin_addr.s_addr = htonl(INADDR_ANY);
	((struct sockaddr_in *)ss)->sin_port        = htons(port);
}

const char *inet_ntop2(const inet_addr_t *ss, char *buf, size_t len)
{
	buf[0] = 0;

#ifdef ENABLE_IPV6
	if (ss->ss_family == AF_INET6)
		return inet_ntop(AF_INET6, &((const struct sockaddr_in6 *)ss)->sin6_addr, buf, len);
#endif
	return inet_ntop(AF_INET, &((const struct sockaddr_in *)ss)->sin_addr, buf, len);
}
