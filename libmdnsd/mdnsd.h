#ifndef MDNSD_H_
#define MDNSD_H_

#include <jmorecfg.h>
#include "mdnsd_config.h"
#include "1035.h"
#include <stdbool.h>

#define QCLASS_IN (1)

/* Main daemon data */
typedef struct mdns_daemon mdns_daemon_t;
/* Record entry */
typedef struct mdns_record mdns_record_t;

/* Answer data */
typedef struct mdns_answer {
	char *name;
	unsigned short int type;
	unsigned long int ttl;
	unsigned short int rdlen;
	unsigned char *rdata;
	struct in_addr ip;	/* A */
	char *rdname;		/* NS/CNAME/PTR/SRV */
	struct {
		//cppcheck-suppress unusedStructMember
		unsigned short int priority, weight, port;
	} srv;			/* SRV */
} mdns_answer_t;

/**
 * Global functions
 */

/**
 * Create a new mdns daemon for the given class of names (usually 1) and
 * maximum frame size
 */
mdns_daemon_t MDNSD_EXPORT * mdnsd_new(int class, int frame);

/**
 * Gracefully shutdown the daemon, use mdnsd_out() to get the last
 * packets
 */
void MDNSD_EXPORT mdnsd_shutdown(mdns_daemon_t *d);

/**
 * Flush all cached records (network/interface changed)
 */
void MDNSD_EXPORT mdnsd_flush(mdns_daemon_t *d);

/**
 * Free given mdns_daemon_t *(should have used mdnsd_shutdown() first!)
 */
void MDNSD_EXPORT mdnsd_free(mdns_daemon_t *d);


/**
 * I/O functions
 */

/**
 * Oncoming message from host (to be cached/processed)
 */
void MDNSD_EXPORT mdnsd_in(mdns_daemon_t *d, struct message *m, unsigned long int ip, unsigned short int port);

/**
 * Outgoing messge to be delivered to host, returns >0 if one was
 * returned and m/ip/port set
 */
int MDNSD_EXPORT mdnsd_out(mdns_daemon_t *d, struct message *m, unsigned long int *ip, unsigned short int *port);

/**
 * returns the max wait-time until mdnsd_out() needs to be called again 
 */
struct timeval MDNSD_EXPORT * mdnsd_sleep(mdns_daemon_t *d);

/**
 * Q/A functions
 */

/**
 * Register a new query
 *
 * The answer() callback is called whenever one is found/changes/expires
 * (immediate or anytime after, mdns_answer_t valid until ->ttl==0)
 * either answer returns -1, or another mdnsd_query() with a %NULL answer
 * will remove/unregister this query
 */
void MDNSD_EXPORT mdnsd_query(mdns_daemon_t *d, char *host, int type, int (*answer)(mdns_answer_t *a, void *arg), void *arg);

/**
 * Returns the first (if last == NULL) or next answer after last from
 * the cache mdns_answer_t only valid until an I/O function is called
 */
mdns_answer_t MDNSD_EXPORT *mdnsd_list(mdns_daemon_t *d, char *host, int type, mdns_answer_t *last);


/**
 * Publishing functions
 */

/**
 * Create a new unique record
 *
 * Call mdnsd_list() first to make sure the record is not used yet.
 *
 * The conflict() callback is called at any point when one is detected
 * and unable to recover after the first data is set_*(), any future
 * changes effectively expire the old one and attempt to create a new
 * unique record
 */
mdns_record_t MDNSD_EXPORT * mdnsd_unique(mdns_daemon_t *d, char *host, unsigned short type, unsigned long ttl, void (*conflict)(char *host, int type, void *arg), void *arg);

/** 
 * Create a new shared record
 */
mdns_record_t MDNSD_EXPORT * mdnsd_shared(mdns_daemon_t *d, char *host, unsigned short type, unsigned long ttl);

/**
 * de-list the given record
 */
void MDNSD_EXPORT mdnsd_done(mdns_daemon_t *d, mdns_record_t *r);

/**
 * These all set/update the data for the given record, nothing is
 * published until they are called
 */
void MDNSD_EXPORT mdnsd_set_raw(mdns_daemon_t *d, mdns_record_t *r, char *data, unsigned short len);
void MDNSD_EXPORT mdnsd_set_host(mdns_daemon_t *d, mdns_record_t *r, char *name);
void MDNSD_EXPORT mdnsd_set_ip(mdns_daemon_t *d, mdns_record_t *r, struct in_addr ip);
void MDNSD_EXPORT mdnsd_set_srv(mdns_daemon_t *d, mdns_record_t *r, unsigned short priority, unsigned short weight, unsigned short port, char *name);

/**
 * Process input queue and output queue. Should be called at least the time which is returned in nextSleep.
 * Returns 0 on success, 1 on read error, 2 on write error
 */
unsigned short MDNSD_EXPORT mdnsd_step(mdns_daemon_t *d, int mdns_socket, bool processIn, bool processOut, struct timeval *nextSleep);

#endif	/* MDNSD_H_ */
