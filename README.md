mdnsd - embeddable Multicast DNS Daemon
=======================================
[![License Badge][]][License] [![Travis Status][]][Travis]

[Jeremie Miller's][jeremie] original mDNS/mDNS-SD library daemon.

Download a [relased tarball][releases] (not a GitHub zip) to unlock a
fully supported version.  Hardcore devs. can proceed to clone the GIT
repository, see below for help.


Usage
-----

mdnsd by default reads service definitions from `/etc/mdns.d/*`, but a
different path can be given, which may be a directory or a single file.

    Usage: mdnsd [-hnv] [-a ADDRESS] [-l LEVEL] [PATH]
    
        -a ADDR   Address of service/host to announce, default: auto
        -h        This help text
        -i IFACE  Interface to announce services on, and get address from
        -l LEVEL  Set log level: none, err, notice (default), info, debug
        -n        Run in foreground, do not detach from controlling terminal
        -p        Persistent mode, retry if the socket or interface is lost
        -t TTL    Set TTL of mDNS packets, default: 1 (link-local only)
        -v        Show program version
    
    Bug report address: https://github.com/troglobit/mdnsd/issues

See the file [API.md][] for pointers on how to use the library.


### Service Records

This section provides a couple of service record examples.  The syntax
of the files is fairly free form.  Optional directives: `name`, `txt`,
`target`, and `cname`.

_FTP service example:_

    # /etc/mdns.d/ftp.service -- mDNS-SD advertisement of FTP service
    name Troglobit FTP Server
    type _ftp._tcp
    port 21
    txt server=uftpd
    txt version=2.6
    target ftp.luthien.local
    cname ftp.local

_HTTP service example:_

    # /etc/mdns.d/http.service -- mDNS-SD advertisement of HTTP service
    name Troglobit HTTP Server
    type _http._tcp
    port 80
    txt server=merecat
    txt version=2.31
    target www.luthien.local
    cname home.local


Build & Install
---------------

This project is built for and developed on GNU/Linux systems, but should
work on any UNIX like system.  Use the standard GNU configure script to
create a Makefile for your system and then call make.

    ./configure
    make all
    make install

Users who checked out the source from GitHub must run `./autogen.sh`
first to create the configure script.  This requires GNU autotools and
`pkg-config` to be installed on the build system.


Origin & References
-------------------

This mDNS-SD implementation was developed by [Jeremie Miller][jeremie]
in 2003, originally [announced on the rendezvous-dev][announced] mailing
list.  It has many forks and has been used by many other applications
over the years.

This GitHub project is an attempt to clean it up, develop it further,
and maintain it for the long haul.


[jeremie]:       https://github.com/quartzjer
[releases]:      https://github.com/troglobit/mdnsd/releases
[announced]:     http://lists.apple.com/archives/rendezvous-dev/2003/Feb/msg00062.html
[API.md]:        https://github.com/troglobit/mdnsd/blob/master/API.md
[License]:       https://en.wikipedia.org/wiki/BSD_licenses
[License Badge]: https://img.shields.io/badge/License-BSD%203--Clause-blue.svg
[Travis]:        https://travis-ci.org/troglobit/mdnsd
[Travis Status]: https://travis-ci.org/troglobit/mdnsd.png?branch=master
