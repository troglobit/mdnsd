/*
 * Copyright (c) 2018-2021  Joachim Wiberg <troglobit@gmail.com>
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
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "mdnsd.h"

struct conf_srec {
	char   *type;

	char   *name;
	int     port;

	char   *target;
	char   *cname;

	char   *txt[42];
	size_t  txt_num;
};


static char *chomp(char *str)
{
	char *p;

	if (!str || strlen(str) < 1) {
		errno = EINVAL;
		return NULL;
	}

	p = str + strlen(str) - 1;
        while (*p == '\n')
		*p-- = 0;

	return str;
}

static int match(char *key, char *token)
{
	return !strcmp(key, token);
}

static void read_line(char *line, struct conf_srec *srec)
{
	char *arg, *token;

	arg = chomp(line);
	DBG("Got line: '%s'", line);

	if (line[0] == '#')
		return;

	token = strsep(&arg, " \t");
	if (!token || !arg) {
		DBG("Skipping, token:%s, arg:%s", token, arg);
		return;
	}

	if (match(token, "type")) {
		free(srec->type);
		srec->type = strdup(arg);
	} else if (match(token, "name")) {
		free(srec->name);
		srec->name = strdup(arg);
	} else if (match(token, "port")) {
		char *end;
		srec->port = (int)strtol(arg, &end, 10);
		if (*end) {
			DBG("Bad port number: %s", arg);
			return;
		}
	} else if (match(token, "target")) {
		free(srec->target);
		srec->target = strdup(arg);
	} else if (match(token, "cname")) {
		free(srec->cname);
		srec->cname = strdup(arg);
	} else if (match(token, "txt") && srec->txt_num < NELEMS(srec->txt)) {
		srec->txt[srec->txt_num++] = strdup(arg);
    }
}

static int parse(char *fn, struct conf_srec *srec)
{
	FILE *fp;
	char line[256];

	DBG("Attempting to read %s ...", fn);
	fp = fopen(fn, "r");
	if (!fp)
		return 1;

	while (fgets(line, sizeof(line), fp))
		read_line(line, srec);
	fclose(fp);
	DBG("Finished reading %s ...", fn);

	return 0;
}

/* Create a new record, or update an existing one */
mdns_record_t *record(mdns_daemon_t *d, int shared, char *host,
		      const char *name, unsigned short type, unsigned long ttl)
{
	mdns_record_t *r;

	r = mdnsd_find(d, name, type);
	while (r != NULL) {
		const mdns_answer_t *a;

		if (!host)
			break;

		a = mdnsd_record_data(r);
		if (!a) {
			r = mdnsd_record_next(r);
			continue;
		}

		if (a->rdname && strcmp(a->rdname, host)) {
			r = mdnsd_record_next(r);
			continue;
		}

		return r;
	}

	if (!r) {
		if (shared)
			r = mdnsd_shared(d, name, type, ttl);
		else
			r = mdnsd_unique(d, name, type, ttl, mdnsd_conflict, NULL);

		if (host)
			mdnsd_set_host(d, r, host);
	}

	return r;
}

static int load(mdns_daemon_t *d, char *path, char *hostname)
{
	struct conf_srec srec;
	unsigned char *packet;
	mdns_record_t *r;
	size_t i;
	xht_t *h;
	char hlocal[256], nlocal[256], tlocal[256];
	int len = 0;

	memset(&srec, 0, sizeof(srec));
	if (parse(path, &srec)) {
		ERR("Failed reading %s: %s", path, strerror(errno));
		return 1;
	}

	if (!srec.name)
		srec.name = strdup(hostname);
	if (!srec.type)
		srec.type = strdup("_http._tcp");

	snprintf(hlocal, sizeof(hlocal), "%s.%s.local.", srec.name, srec.type);
	snprintf(nlocal, sizeof(nlocal), "%s.local.", srec.name);
	snprintf(tlocal, sizeof(tlocal), "%s.local.", srec.type);
	if (!srec.target)
		srec.target = strdup(hlocal);

	/* Announce that we have a $type service */
	record(d, 1, tlocal, DISCO_NAME, QTYPE_PTR, 120);
	record(d, 1, srec.target, tlocal, QTYPE_PTR, 120);

	r = record(d, 0, NULL, hlocal, QTYPE_SRV, 120);
	mdnsd_set_srv(d, r, 0, 0, srec.port, nlocal);

	r = record(d, 0, NULL, nlocal, QTYPE_A, 120);
	mdnsd_set_ip(d, r, mdnsd_get_address(d));

	if (srec.cname)
		record(d, 1, srec.cname, nlocal, QTYPE_CNAME, 120);
	r = record(d, 0, NULL, hlocal, QTYPE_TXT, 4500);

	h = xht_new(11);
	for (i = 0; i < srec.txt_num; i++) {
		char *ptr;

		ptr = strchr(srec.txt[i], '=');
		if (!ptr)
			continue;
		*ptr++ = 0;

		xht_set(h, srec.txt[i], ptr);
	}
	packet = sd2txt(h, &len);
	xht_free(h);
	mdnsd_set_raw(d, r, (char *)packet, len);
	free(packet);

	free(srec.type);
	free(srec.name);
	free(srec.target);
	free(srec.cname);
	for (i = 0; i < NELEMS(srec.txt); i++) {
		free(srec.txt[i]);
	}

	return 0;
}

int conf_init(mdns_daemon_t *d, char *path, int hostid)
{
	char hostname[HOST_NAME_MAX];
	struct stat st;
	int rc = 0;

	/* apparently gethostname() can fail ... */
	if (gethostname(hostname, sizeof(hostname)) == -1)
		strlcpy(hostname, "default", sizeof(hostname));

	/* uniqify hostname by appending -hostid, e.g., default-2 */
	if (hostid > 1) {
		size_t hlen, slen;
		char suffix[16];

		slen = snprintf(suffix, sizeof(suffix), "-%d", hostid) + 1;
		hlen = strlen(hostname);
		if (hlen + slen >= sizeof(hostname))
			hlen = sizeof(hostname) - slen;

		strlcpy(&hostname[hlen], suffix, sizeof(hostname) - hlen);
	}

	if (stat(path, &st)) {
		if (ENOENT == errno)
			ERR("Services directory %s, missing or unconfigured.", path);
		else
			ERR("Cannot determine path type: %s", strerror(errno));
		return 1;
	}

	if (S_ISDIR(st.st_mode)) {
		glob_t gl;
		size_t i;
		char pat[strlen(path) + 64];
		int flags = GLOB_ERR;

		strlcpy(pat, path, sizeof(pat));
		if (pat[strlen(path) - 1] != '/')
			strcat(pat, "/");
		strcat(pat, "*.service");

#ifdef GLOB_TILDE
		/* E.g. musl libc < 1.1.21 does not have this GNU LIBC extension  */
		flags |= GLOB_TILDE;
#else
		/* Simple homegrown replacement that at least handles leading ~/ */
		if (!strncmp(pat, "~/", 2)) {
			const char *home;

			home = getenv("HOME");
			if (home) {
				memmove(pat + strlen(home), pat, strlen(path));
				memcpy(pat, home, strlen(home));
			}
		}
#endif

		if (glob(pat, flags, NULL, &gl)) {
			ERR("No .service files found in %s", pat);
			return 1;
		}

		for (i = 0; i < gl.gl_pathc; i++)
			rc |= load(d, gl.gl_pathv[i], hostname);

		globfree(&gl);
	} else
		rc |= load(d, path, hostname);

	return rc;
}

