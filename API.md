Library
-------

There are several use-cases for this project, the daemon provides one
use of the library, yet many others are possible.

Here is a small example how to publish a few records:

	char hlocal[384];
	char nlocal[384];
	char hostname[256];
	char *path = "/path/to/service/"

	gethostname(hostname, sizeof(hostname));

	sprintf(hlocal, "%s._http._tcp.local.", hostname);
	sprintf(nlocal, "%s.local.", hostname);

	/* Announce that we have a _http._tcp service */
	r = mdnsd_shared(d, "_services._dns-sd._udp.local.", QTYPE_PTR, 120);
	mdnsd_set_host(d, r, "_http._tcp.local.");

	r = mdnsd_shared(d, "_http._tcp.local.", QTYPE_PTR, 120);
	mdnsd_set_host(d, r, hlocal);
	r = mdnsd_unique(d, hlocal, QTYPE_SRV, 600, conflict, NULL);
	mdnsd_set_srv(d, r, 0, 0, port, nlocal);
	r = mdnsd_unique(d, nlocal, QTYPE_A, 600, conflict, NULL);
	mdnsd_set_raw(d, r, (char *)&ip.s_addr, 4);

	r = mdnsd_unique(d, hlocal, QTYPE_TXT, 600, conflict, NULL);
	h = xht_new(11);
	if (path && strlen(path))
		xht_set(h, "path", path);
	packet = sd2txt(h, &len);
	xht_free(h);
	mdnsd_set_raw(d, r, (char *)packet, len);
	free(packet);

How to read a previously published record:

	r = mdnsd_get_published(d, "_http._tcp.local.");
	while (r) {
		const mdns_answer_t *data;

		data = mdnsd_record_data(r);
		if (data)
			DBG("Found record of type %d", data->type);

		r = mdnsd_record_next(r);
	}

