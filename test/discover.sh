#!/bin/sh
# Set up basic topology, start mdnsd and then use mqeuery to locate it.
#set -x

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

topo basic
mdnsd
discover

OK
