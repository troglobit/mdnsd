mdnsd Test Suite
================

Tests are run by `make check` and come in two flavors:

- **Regression tests** (`*.sh`) start `mdnsd` and an `mquery` client in
  separate network namespaces and verify real mDNS exchanges
- **Unit tests** (`xht`, `addr`) are [cmocka][] programs that exercise
  individual translation units directly

Running
-------

Build with tests enabled, then run the whole suite:

```console
$ ./configure --enable-tests
$ make check
...
```

Run a single test by name (works for both flavours):

```console
$ make check TESTS=discover.sh
...
$ make check TESTS=addr
...
```

Requirements
------------

- `libcmocka` (`libcmocka-dev`) for the unit tests
- `iproute2` and `util-linux` (`unshare`, `nsenter`) for the network
  namespace setup the regression tests rely on
- `tshark` is optional, used only by tests that capture a pcap

The regression tests run inside a mount + user namespace
(`unshare -mrun --map-auto`), so they can create network namespaces
without root.  On distributions that restrict unprivileged user
namespaces (recent Ubuntu), enable them first:

```console
$ sudo sysctl kernel.apparmor_restrict_unprivileged_userns=0
...
```console

[cmocka]: https://cmocka.org/
