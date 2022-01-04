mdnsd - embeddable Multicast DNS Daemon
=======================================
[![License svg][]][License] [![Repology svg][]][Repology] [![GitHub svg][]][GitHub] [![Coverity svg][]][Coverity]

- [About](#about)
- [Usage](#usage)
  - [Service Records](#service-records)
- [Build & Install](#build--install)
- [Origin & References](#origin--references)


About
-----

[Jeremie Miller's][jeremie] original mDNS/mDNS-SD library daemon.

> Download a [versioned relased tarball][releases] (not a GitHub zip) to
> unlock a fully supported version.  Hardcore devs. can proceed to clone
> the GIT repository, see below for help.


Usage
-----

mdnsd by default reads service definitions from `/etc/mdns.d/*`, but a
different path can be given, which may be a directory or a single file.

    Usage: mdnsd [-hnpsv] [-i IFACE] [-l LEVEL] [-t TTL] [PATH]
    
        -h        This help text
        -i IFACE  Announce services only on this interface, default: all
        -l LEVEL  Set log level: none, err, notice (default), info, debug
        -n        Run in foreground, do not detach from controlling terminal
        -s        Use syslog even if running in foreground
        -t TTL    Set TTL of mDNS packets, default: 1 (link-local only)
        -v        Show program version
    
    Bug report address: https://github.com/troglobit/mdnsd/issues

By default mdnsd daemonizes, detaches from the controlling terminal and
continues running in the background, logging errors (or debug messages
if enabled) to the systmem log.  There is no output to be expected.  On
GNU/Linux, use `mdns-scan`, the bundled `mquery` tool, or Wireshark to
verify your setup.  Other operating systems have their own set of tools
for mDNS-SD and mdnsd may not even have a place there.

mdnsd runs on all multicast enabled system interfaces.  It can be limted
to run on only one using the `-i IFACE` command line option.  Starting
mdnsd early in the boot process, when the interface may not yet have
acquired an IP address, or the interface itself may not even exist yet,
mdnsd handles this by periodically probing for interface changes.

See the file [API.md][] for pointers on how to use the mDNS library.


### Service Records

This section provides a couple of service record examples.  The syntax
of the files is fairly free form.  Optional directives: `name`, `txt`,
`target`, and `cname`.

> **Note:** you need at least one service record for `mdnsd` to respond
> to queries from, e.g., `mdns-scan`.

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

_SSH service example:_

    # /etc/mdns.d/http.service -- mDNS-SD advertisement of SSH service
    name Dropbear SSH Server
    type _ssh._tcp
    port 22


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

If you install to the default location used by the configure script,
the library is installed in `/usr/local/lib`, which may not be in
the default search path for your system.  Depending on the C library
used, the file `/etc/ld.so.conf` may exist (there may also be a
sub-directory).  If `/usr/local/lib` is already listed there, you
may need to update the cache:

    ldconfig -v |grep mdnsd

If you don't get any output from the above command, the ld.so.conf needs
updating, or you may not be using the GNU C library.


Origin & References
-------------------

This mDNS-SD implementation was developed by [Jeremie Miller][jeremie]
in 2003, originally [announced on the rendezvous-dev][announced] mailing
list.  The original name was 'mhttp'.  It has many forks and has been
used by many other applications over the years.

The GitHub project is an attempt by [Joachim Wiberg][troglobit] to clean
up the original code base, develop it further, and maintain it for the
long haul under the name `mdnsd`.

[jeremie]:      https://github.com/quartzjer
[troglobit]:    https://github.com/troglobit
[releases]:     https://github.com/troglobit/mdnsd/releases
[announced]:    https://web.archive.org/web/20140115142008/http://lists.apple.com/archives/rendezvous-dev/2003/Feb/msg00062.html
[API.md]:       https://github.com/troglobit/mdnsd/blob/master/API.md
[License]:      https://en.wikipedia.org/wiki/BSD_licenses
[License svg]:  https://img.shields.io/badge/License-BSD%203--Clause-blue.svg
[Repology]:     https://repology.org/project/mdnsd/versions
[Repology svg]: https://repology.org/badge/tiny-repos/mdnsd.svg
[GitHub]:       https://github.com/troglobit/mdnsd/actions/workflows/build.yml/
[GitHub svg]:   https://github.com/troglobit/mdnsd/actions/workflows/build.yml/badge.svg
[Coverity]:     https://scan.coverity.com/projects/20680
[Coverity svg]: https://scan.coverity.com/projects/20680/badge.svg
