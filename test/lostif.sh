#!/bin/sh
# Verify mdnsd does not crash when an interface is lost (removed).
#set -x

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

topo basic

mdnsd
discover

print "Deleting dummy1 interface ..."
# shellcheck disable=SC2154
nsenter --net="$server" -- ip link del dummy1

# Recheck period is 10 sec, retry test
sleep 10
discover

OK
