#!/bin/sh
# Set up basic topology, start mdnsd and then use mqeuery to locate it.
#set -x

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

topo basic
mdnsd

print "Starting mquery to locate mdnsd ..."
mquery >"$DIR/result" || FAIL "Not found"

# shellcheck disable=SC2154
grep -q "+ _ftp._tcp.local. ($server_addr)" "$DIR/result" || FAIL

OK
