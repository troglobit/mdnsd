mdnsd - embeddable Multicast DNS Daemon
=======================================
[![License Badge][]][License] [![Travis Status][]][Travis]

This is [Jeremie Miller's][jeremie] original mDNS/mDNS-SD library and
example daemon.  The project is open source software under the modified
[3-clause BSD license][License].


Usage
-----

    Usage: mdnsd [-hv] [-a ADDR ] [-f FILE] [-l LEVEL]
    
        -a ADDR   Address of service/host to announce, default: auto
        -f FILE   Read service data from FILE, default: /etc/mdns.d/*
        -h        This help text
        -l LEVEL  Set log level: none, err, info (default), debug
        -n        Run in foreground, do not detach from controlling terminal
        -v        Show program version
    
    Bug report address: https://github.com/troglobit/mdnsd/issues


Running
-------

To test the included example applications you need to first start the
`mdnsd` daemon before calling `mquery`:

    mdnsd &
    mquery 12 _http._tcp.local.


Library
-------

There are several use-cases for this project, the bundled daemon provides
some example uses of the library, and many others are possible.  Here is
a small example:

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



Build & Install
---------------

The software is built for and developed on GNU/Linux systems, but should
work on any UNIX like system.

The GNU configure and build system is used, simply call the configure
script to generate a Makefile.  If you are using the GitHub sources you
first need to call `./autogen.sh` to generate the configure script.

    ./configure
    make all
    make install


Origin & References
-------------------

This MDNS-SD implementation was developed by [Jeremie Miller][jeremie]
in 2003, originally [announced on the rendezvous-dev][announced] mailing
list.  It has many forks and has been used by many other applications
over the years.

This GitHub project is an attempt to clean it up, develop it further,
and maintain it for the long haul.


[jeremie]:       https://github.com/quartzjer
[announced]:     http://lists.apple.com/archives/rendezvous-dev/2003/Feb/msg00062.html
[License]:       https://en.wikipedia.org/wiki/BSD_licenses
[License Badge]: https://img.shields.io/badge/License-BSD%203--Clause-blue.svg
[Travis]:        https://travis-ci.org/troglobit/mdnsd
[Travis Status]: https://travis-ci.org/troglobit/mdnsd.png?branch=master
