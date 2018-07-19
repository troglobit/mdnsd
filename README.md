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
        -v        Show program version
    
    Bug report address: https://github.com/troglobit/mdnsd/issues


Running
-------

To test the included example applications you need to first start the
`mdnsd` daemon before calling `mquery`:

    mdnsd &
    mquery 12 _http._tcp.local.


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
