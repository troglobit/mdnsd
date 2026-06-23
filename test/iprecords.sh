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



print "Removing all IPv6 addresses from topology while mdnsd is running..."
# Start capturing before the flush: mdnsd reacts to the netlink event
# almost immediately, so a collector started afterwards misses the goodbye.
collect
nsenter --net="$server" -- ip -6 addr flush dev eth0
print "Waiting for mdnsd to send goodbye ..."
# With netlink the goodbye is near-instant; wait past the 10s address
# poll so the test still passes where netlink is unavailable.
sleep 12
stop_collect

# A goodbye (TTL 0) for the removed global address must be sent.  The
# host may advertise several addresses (e.g. link-local + global), so
# match the global goodbye specifically rather than the whole packet.
if [ -f "$DIR/pcap" ] ; then
	tshark -r "$DIR/pcap" -Y "mdns && dns.resp.ttl == 0 && dns.aaaa == ${server_addr_6}" 2>/dev/null \
		| grep -q . || FAIL "No goodbye (TTL 0) sent for ${server_addr_6}"
else
	echo "Unable to verify goodbye packets being sent"
	if ! command -v tshark &>/dev/null ; then
		echo "Program tshark is not installed. Cannot collect network packets."
	fi
fi

print "Starting mquery to query for A records ..."
mquery -s -t 1 test.local. >"$DIR/result" || FAIL "Not found"
# shellcheck disable=SC2154
grep -q "A test.local. .* $server_addr" "$DIR/result" || FAIL

print "Starting mquery to query for AAAA records ..."
mquery -s -t 28 test.local. >"$DIR/result" || FAIL
# shellcheck disable=SC2154
grep -q "AAAA test.local. " "$DIR/result" && FAIL "No AAAA records should be sent anymore"


OK
