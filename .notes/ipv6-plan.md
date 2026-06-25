# IPv6 support (issue #10)

## Goal

Make mdnsd actually speak mDNS over IPv6 — join `ff02::fb`, send and
receive over v6 — not merely advertise AAAA records inside IPv4
responses.

## Status before this work

- **Records:** A + AAAA advertised, multiple addresses per interface,
  IPv6 address selection in `src/addr.c`.  Done.
- **Transport:** IPv4 only.  Sockets are `AF_INET`/`224.0.0.251`;
  `mdnsd_in()`/`mdnsd_out()` take `struct in_addr`.  Missing.

## Architecture: per-family contexts, one process

Each interface runs the mDNS state machine once per address family — a
v4 context+socket (`224.0.0.251`) and a v6 context+socket (`ff02::fb`),
both publishing the same A+AAAA records so a query on either transport
returns both.  Everything stays inside the single `mdnsd` process and
its one `select()` loop.

`mdns_daemon_t` is an in-process struct (record tables + send queues),
not an OS process; mdnsd already keeps one per interface today.  IPv6
just adds a second per interface.  systemd/finit supervise the same
single PID.

libmdnsd's announce/probe/answer state machine stays single-family and
untouched; only the transport address type generalizes and each context
learns its family.

Rejected: one dual-stack context driving both sockets — that forces
`mdnsd_out()` to emit per-family packets from shared queues, entangling
the state machine with transport (the overhaul we want to avoid).

## Reuse from uftpd (commit 4751b57, "Fix #40: Add IPv6 support")

- Copy `src/inet.[ch]` (the `sockaddr_storage` wrapper `inet_addr_t` plus
  helpers) into `libmdnsd/`.
- The `open_socket()` / `IPV6_V6ONLY` pattern, adapted for multicast.
- Mirror `test/ipv6.sh`.

## Changes

libmdnsd:
- `inet.[ch]` (new, from uftpd) — shared, since the public API uses it.
- `mdns_daemon_t` gains a transport family; `mdnsd_new()` takes it.
- `mdnsd_in()`/`mdnsd_out()`, `process_in`/`process_out`, and the
  unicast-answer store move from `struct in_addr` to `inet_addr_t`.
  **Public API + ABI break** — bump the libtool `-version-info`.
- `mdnsd_out()` selects the multicast group (`224.0.0.251` / `ff02::fb`)
  by the context's family.

src (daemon):
- v6 multicast socket helper (join `ff02::fb`, `IPV6_MULTICAST_IF/HOPS/
  LOOP`, `IPV6_V6ONLY`, bind `[::]:5353`).
- `struct iface` gains `sd6` + `mdns6`; the main loop `select()`s both;
  `conf_init()` + `mdnsd_set_interface_addresses()` run for both contexts.

src (mquery): v6 socket + `inet_addr_t` loop.

build: `--enable-ipv6`, default **on** (like uftpd); `ENABLE_IPV6` guards.

tests: `test/ipv6.sh` — query mdnsd over `ff02::fb`, assert PTR/SRV/AAAA
answered over the v6 transport.

## Landing

Implemented in phases for verification but landed as a single commit
(as uftpd did), so the review/`/simplify` pass sees the whole change.
