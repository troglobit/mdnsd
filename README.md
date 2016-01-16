mdnsd - embeddable Multicast DNS Daemon
=======================================
[![Travis Status][]][Travis]

This package is intended for software developers and integrators, there
isn't really anything here for an end user.  The project license is the
modified 3-clause BSD, https://en.wikipedia.org/wiki/BSD_licenses

You should be able to just type make and it will build the included
example apps.  Otherwise, check out `mdnsd.h` to get started, the API is
as simple as I could make it, but I hope to find some easier/better ways
to improve it in the future.  Also included are some other utilities,
`sdtxt.*` for service discovery TXT record parsing/generation, and
`xht.*` for simple fast hashtables, and `1035.*` which mdnsd uses for
standalone DNS parsing.


Origin & References
-------------------

This MDNS-SD implementation was developed by [Jeremie Miller][[jeremie]]
in 2003 and originally [announced on the rendezvous-dev][announced]
mailing list.  It has many forks and has been used by many other
applications over the years.

This GitHub project is an attempt to clean it up, develop it further,
and maintain it for the long haul.

[jeremie]:       https://github.com/quartzjer
[announced]:     http://lists.apple.com/archives/rendezvous-dev/2003/Feb/msg00062.html
[Travis]:        https://travis-ci.org/troglobit/mdnsd
[Travis Status]: https://travis-ci.org/troglobit/mdnsd.png?branch=master
