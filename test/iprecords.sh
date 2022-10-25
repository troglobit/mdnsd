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
nsenter --net="$server" -- ip -6 addr flush dev eth0
collect
print "Waiting 15 seconds for mdnsd to catch up..."
sleep 15
stop_collect

# Check that Goodbye packets (TTL == 0) were sent
if [ -f "$DIR/pcap" ] ; then
	response=$(tshark -r "$DIR/pcap" -Y mdns -T fields -e dns.resp.ttl -e dns.aaaa 2>&1 | grep -v "root")
	[ -n "$response" ] || FAIL "No mDNS goodbye packets found"
	[ "${response}" = "0,0	${server_addr_6},${server_addr_6}" ] || FAIL "mDNS packets did not match goodbye packet requirements"

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
