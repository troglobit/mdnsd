Change Log
==========

All relevant changes to the project are documented in this file.


[v0.11][] - 2022-01-09
----------------------

Multiple interface support, and a lot of fixes again from the team at
[devolo AG](https://www.devolo.com).

### Changes
- Add support for multiple interfaces.  Similar to running one
  mdnsd v0.10 per interface, sharing the same .conf files, issue #8
- Drop `-a ADDR` command line option, not applicable anymore
- Drop `-p` command line option, not needed anymore
- The `-i IFACE` option now limits mdnsd to run on one interface
- Add support for query type ANY, by Peter Fleer, issue #34
- The libmdnsd function `mdnsd_step()` is now always non-blocking by
  employing `MSG_DONTWAIT` for its `sendto()` and `recvfrom()` ops
- Create PID file when starting up, and touch on .conf reload.  This
  is used by process supervisors like Finit to acknowledge that the
  process (mdnsd) is done and ready to serve requests again
- Updated mquery tool to behave a bit more like mdns-scan(1), except
  with more command line options.  mquery still only runs on one
  interface, unlike mdnsd itself

### Fixes
- Fix #33: multiple code cleanups and minor fixes by Wolfgang Rösler
- Fix #35: validate port number in .conf file
- Fix #38: possible NULL pointer dereference and memory leaks when
  reading .conf files, by Wolfgang Rösler
- Fix #39: multiple fixes by Peter Fleer
  - Fix conflict detection
  - If a conflict is detected, append `-index` suffix to hostname
    to uniqify the name -- this happens in multi-deploy scenarios
  - When reloading .conf files, delete previously published records
  - Drop own, looped back, multicast packets -- only if we detect
    it as our own published records
  - Send publish records only if probes have gone unanswered
- Fix #43: _ldecomp() breaking on long name offsets, by Chris Beaumont


[v0.10][] - 2020-05-06
----------------------

Bug fix release, thanks to [devolo AG](https://www.devolo.com).

### Changes

- Update man page and add `mdnsd.service.5` to document how to write
  service record files.

### Fixes

- Memory leak fixes courtesy of Peter Fleer and Wolfram Rösler
- Append text field to the TXT record instead of the A record, in the
  case of a PTR type, the name must also match.  Found and fixed by
  Peter Fleer
- Fix potential endless loop when decoding message labels, found and
  fixed by Wolfram Rösler
- Skip message processing when the packet parser failed, found and
  fixed by Wolfram Rösler
- Misc. fuzzer fixes, found and fixed by Wolfram Rösler


[v0.9][] - 2020-04-02
---------------------

Bug fix and license clarification release.

### Changes
- Update Debian packaging, split into several packages
- Minor updates to README
- Add manual pages mdnsd(8) and mquery(1)
- Clarify placeholders in BSD-3 license, spotted by Thomas Bong

### Fixes
- Fix #20: segfault that may occur when a new node is allocated, by
  Colin MacKenzie IV
- Fix #22: check for both name *and* type in `mdnsd_find()`, otherwise
  any previous name matching may be considered an existing node, found
  and fixed by Thomas Bong
- Fix #23: possible NULL pointer dereference when comparing strings


[v0.8][] - 2018-08-23
---------------------

Huge thanks to Jeremie Miller for the original mDNS implementation, to
Stefan Profanter for fixing the code from the early days, and also to
Thom Nichols for careful testing and nudging me to finalize the work and
get a proper release out there.

Apologies for the terse change log, there are a lot of changes to Jer's
original, in a sense this is a brand new project.  This is a pre-release
of the upcoming v1.0 with some important to remember limitations:

- no IPv6,
- only handles one interface (no multi-homing), and
- only one A record can be announced per service

### Changes
- Added support for systemd unit file
- Added example SSH service record in `/etc/mdns.d/ssh.service`
- Added support for building Debian/Ubuntu `.deb` packages
- Renamed example mhttp appliation to mdnsd
- Added support for running as a proper UNIX daemon
- Added support for logging to syslog
- Added proper option parsing
- Added configuration file support, or rather DNS record file support
- Added GNU configure & build system to ease building and portability
- Added support for mDNS-SD service discovery/enumeration, i.e., the
  `mdns-scan` tool finds and reports services mdnsd announces
- Added support for announcing multiple service records

### Fixes
- Build fixes for recent versions of clang and gcc
- Build fixes for Alpine Linux and other musl libc based systems
- Default IP TTL value for multicast frames (224.0.0.251) is now 1
- Fixed endian problems and other issues with `mdns_set_ip()`
- Fixed triggering of unicast reply to mDNS query due to missing `ntohs()`
- Fixed service record TTLs; 120 and 4500 are RFC recommended values
- Fixed memory leaks

[UNRELEASED]: https://github.com/troglobit/mdnsd/compare/v0.10...HEAD
[v0.11]: https://github.com/troglobit/mdnsd/compare/v0.10...v0.11
[v0.10]: https://github.com/troglobit/mdnsd/compare/v0.9...v0.10
[v0.9]: https://github.com/troglobit/mdnsd/compare/v0.8...v0.9
[v0.8]: https://github.com/troglobit/mdnsd/compare/v0.7G...v0.8
