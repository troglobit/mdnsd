#!/bin/sh
# Query mdnsd from the same host it runs on.
#
# mdnsd_in() used to drop every packet from one of our own addresses, so a
# querier sharing the host with the responder saw nothing.  Commit 3fc7df1
# replaced that blanket drop with a looped-back check at the conflict sites
# only, precisely to allow a local scan; verify the responder is visible
# from its own host, and that mquery -L opts back out (like avahi-browse -l).
#set -x

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

topo basic
mdnsd

print "Querying on the mdnsd host, expecting to find the local responder ..."
mquery_local >"$DIR/result" || FAIL "Query failed"
# shellcheck disable=SC2154
grep -q "+ _ftp._tcp.local. ($server_addr)" "$DIR/result" \
	|| FAIL "Local responder not discoverable from its own host"

print "Querying with -L, expecting the local responder to be ignored ..."
mquery_local -L >"$DIR/nolocal" || FAIL "Query failed"
grep -q "+ _ftp._tcp.local. ($server_addr)" "$DIR/nolocal" \
	&& FAIL "-L did not ignore the local responder"

OK
