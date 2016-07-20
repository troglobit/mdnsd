/* Familiarize yourself with RFC1035 if you want to know what all the
 * variable names mean.  This file hides most of the dirty work all of
 * this code depends on the buffer space a packet is in being 4096 and
 * zero'd before the packet is copied in also conveniently decodes srv
 * rr's, type 33, see RFC2782
 */
#ifndef MDNS_1035_H_
#define MDNS_1035_H_

#include "mdnsd_config.h"

#ifdef _WIN32
# ifdef SLIST_ENTRY
#  undef SLIST_ENTRY /* Fix redefinition of SLIST_ENTRY on mingw winnt.h */
# endif
# include <winsock2.h>
# include <ws2tcpip.h>

#ifndef in_addr_t
#define in_addr_t unsigned __int32
#endif
#else
# include <arpa/inet.h>
#endif

/* Should be reasonably large, for UDP */
#define MAX_PACKET_LEN 4000
#define MAX_NUM_LABELS 20

struct question {
	char *name;
	unsigned short int type, class;
};

#define QTYPE_A      1
#define QTYPE_NS     2
#define QTYPE_CNAME  5
#define QTYPE_PTR    12
#define QTYPE_TXT    16
#define QTYPE_SRV    33

struct resource {
	char *name;
	unsigned short int type, class;
	unsigned long int ttl;
	unsigned short int rdlength;
	unsigned char *rdata;
	union {
		struct {
			struct in_addr ip;
			//cppcheck-suppress unusedStructMember
			char *name;
		} a;
		struct {
			//cppcheck-suppress unusedStructMember
			char *name;
		} ns;
		struct {
			//cppcheck-suppress unusedStructMember
			char *name;
		} cname;
		struct {
			//cppcheck-suppress unusedStructMember
			char *name;
		} ptr;
		struct {
			//cppcheck-suppress unusedStructMember
			unsigned short int priority, weight, port;
			//cppcheck-suppress unusedStructMember
			char *name;
		} srv;
	} known;
};

struct message {
	/* External data */
	unsigned short int id;
	struct {
		//cppcheck-suppress unusedStructMember
		unsigned short qr:1, opcode:4, aa:1, tc:1, rd:1, ra:1, z:3, rcode:4;
	} header;
	unsigned short int qdcount, ancount, nscount, arcount;
	struct question *qd;
	struct resource *an, *ns, *ar;

	/* Internal variables */
	unsigned char *_buf;
	char *_labels[MAX_NUM_LABELS];
	int _len, _label;

	/* Packet acts as padding, easier mem management */
	unsigned char _packet[MAX_PACKET_LEN];
};

/**
 * Returns the next short/long off the buffer (and advances it)
 */
unsigned short int net2short(unsigned char **buf);
unsigned long int  net2long (unsigned char **buf);

/**
 * copies the short/long into the buffer (and advances it)
 */
MDNSD_EXPORT void short2net(unsigned short int i, unsigned char **buf);
MDNSD_EXPORT void long2net (unsigned long int  l, unsigned char **buf);

/**
 * parse packet into message, packet must be at least MAX_PACKET_LEN and
 * message must be zero'd for safety
 */
MDNSD_EXPORT void message_parse(struct message *m, unsigned char *packet);

/**
 * create a message for sending out on the wire
 */
struct message *message_wire(void);

/**
 * append a question to the wire message
 */
MDNSD_EXPORT void message_qd(struct message *m, char *name, unsigned short int type, unsigned short int class);

/**
 * append a resource record to the message, all called in order!
 */
MDNSD_EXPORT void message_an(struct message *m, char *name, unsigned short int type, unsigned short int class, unsigned long int ttl);
MDNSD_EXPORT void message_ns(struct message *m, char *name, unsigned short int type, unsigned short int class, unsigned long int ttl);
MDNSD_EXPORT void message_ar(struct message *m, char *name, unsigned short int type, unsigned short int class, unsigned long int ttl);

/**
 * Append various special types of resource data blocks
 */
MDNSD_EXPORT void message_rdata_long (struct message *m, struct in_addr l);
MDNSD_EXPORT void message_rdata_name (struct message *m, char *name);
MDNSD_EXPORT void message_rdata_srv  (struct message *m, unsigned short int priority, unsigned short int weight,
			 unsigned short int port, char *name);
MDNSD_EXPORT void message_rdata_raw  (struct message *m, unsigned char *rdata, unsigned short int rdlength);

/**
 * Return the wire format (and length) of the message, just free message
 * when done
 */
MDNSD_EXPORT unsigned char *message_packet     (struct message *m);
MDNSD_EXPORT int            message_packet_len (struct message *m);

#endif	/* MDNS_1035_H_ */
