#!/bin/sh
# Set up basic topology, start mdnsd and then use mqeuery to query for A records.
#set -x

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

topo basic
mdnsd


print "Starting mquery to query for A records ..."
mquery -s -t 1 test.local. >"$DIR/result" || FAIL "Not found"
# shellcheck disable=SC2154
grep -q "A test.local. .* $server_addr" "$DIR/result" || FAIL

mdnsd_stop


print "Adding IPv6 address to topology ..."
nsenter --net="$server" -- ip -6 addr add "${server_addr_6}"/64 dev eth0
mdnsd


print "Starting mquery to query for A records ..."
mquery -s -t 1 test.local. >"$DIR/result" || FAIL "Not found"
# shellcheck disable=SC2154
grep -q "A test.local. .* $server_addr" "$DIR/result" || FAIL

print "Starting mquery to query for AAAA records ..."
mquery -s -t 28 test.local. >"$DIR/result" || FAIL "Not found"
# shellcheck disable=SC2154
grep -q "AAAA test.local. .* $server_addr_6" "$DIR/result" || FAIL


OK
