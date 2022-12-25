#!/bin/sh
# Verify mdnsd does not crash when an interface is lost (removed).
#set -x

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

topo basic

mdnsd
discover

print "Deleting eth0 interface ..."
# shellcheck disable=SC2154
nsenter --net="$server" -- ip link del eth0
sleep 5

# Verify we don't segfault on loss of interface, issue #74
pgrep mdnsd || FAIL

print "Restoring eth0 ..."
topo basic
sleep 10

print "Rechecking mDNS connectivity"
pgrep mdnsd
discover

OK
