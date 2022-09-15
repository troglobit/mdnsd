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


OK
