#ifndef MDNS_SDTXT_H_
#define MDNS_SDTXT_H_
#include "xht.h"

/**
 * returns hashtable of strings from the SD TXT record rdata
 */
xht_t *txt2sd(unsigned char *txt, int len);

/**
 * returns a raw block that can be sent with a SD TXT record, sets length
 */
unsigned char *sd2txt(xht_t *h, int *len);

#endif	/* MDNS_SDTXT_H_ */
