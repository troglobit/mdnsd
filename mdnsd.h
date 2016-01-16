#ifndef mdnsd_h
#define mdnsd_h
#include "1035.h"
#include <arpa/inet.h>
#include <sys/time.h>

// main daemon data
typedef struct mdns_daemon mdns_daemon_t;
// record entry
typedef struct mdns_record mdns_record_t;

// answer data
typedef struct mdns_answer {
	unsigned char *name;
	unsigned short int type;
	unsigned long int ttl;
	unsigned short int rdlen;
	unsigned char *rdata;
	struct in_addr ip;	// A
	unsigned char *rdname;	// NS/CNAME/PTR/SRV
	struct {
		unsigned short int priority, weight, port;
	} srv;			// SRV
} mdns_answer_t;

///////////
// Global functions
//
// create a new mdns daemon for the given class of names (usually 1) and maximum frame size
mdns_daemon_t *mdnsd_new(int class, int frame);

//
// gracefully shutdown the daemon, use mdnsd_out() to get the last packets
void mdnsd_shutdown(mdns_daemon_t *d);

//
// flush all cached records (network/interface changed)
void mdnsd_flush(mdns_daemon_t *d);

//
// free given mdns_daemon_t *(should have used mdnsd_shutdown() first!)
void mdnsd_free(mdns_daemon_t *d);

//
///////////

///////////
// I/O functions
//
// incoming message from host (to be cached/processed)
void mdnsd_in(mdns_daemon_t *d, struct message *m, unsigned long int ip, unsigned short int port);

//
// outgoing messge to be delivered to host, returns >0 if one was returned and m/ip/port set
int mdnsd_out(mdns_daemon_t *d, struct message *m, unsigned long int *ip, unsigned short int *port);

//
// returns the max wait-time until mdnsd_out() needs to be called again 
struct timeval *mdnsd_sleep(mdns_daemon_t *d);

//
////////////

///////////
// Q/A functions
// 
// register a new query
//   answer(record, arg) is called whenever one is found/changes/expires (immediate or anytime after, mdns_answer_t valid until ->ttl==0)
//   either answer returns -1, or another mdnsd_query with a NULL answer will remove/unregister this query
void mdnsd_query(mdns_daemon_t *d, char *host, int type, int (*answer) (mdns_answer_t *a, void *arg), void *arg);

//
// returns the first (if last == NULL) or next answer after last from the cache
//   mdns_answer_t only valid until an I/O function is called
mdns_answer_t *mdnsd_list(mdns_daemon_t *d, char *host, int type, mdns_answer_t *last);

//
///////////

///////////
// Publishing functions
//
// create a new unique record (try mdnsda_list first to make sure it's not used)
//   conflict(arg) called at any point when one is detected and unable to recover
//   after the first data is set_*(), any future changes effectively expire the old one and attempt to create a new unique record
mdns_record_t *mdnsd_unique(mdns_daemon_t *d, char *host, int type, long int ttl, void (*conflict) (char *host, int type, void *arg), void *arg);

// 
// create a new shared record
mdns_record_t *mdnsd_shared(mdns_daemon_t *d, char *host, int type, long int ttl);

//
// de-list the given record
void mdnsd_done(mdns_daemon_t *d, mdns_record_t *r);

//
// these all set/update the data for the given record, nothing is published until they are called
void mdnsd_set_raw(mdns_daemon_t *d, mdns_record_t *r, char *data, int len);
void mdnsd_set_host(mdns_daemon_t *d, mdns_record_t *r, char *name);
void mdnsd_set_ip(mdns_daemon_t *d, mdns_record_t *r, struct in_addr ip);
void mdnsd_set_srv(mdns_daemon_t *d, mdns_record_t *r, int priority, int weight, int port, char *name);

//
///////////


#endif
