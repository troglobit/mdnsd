#!/bin/sh
# Verify mdnsd speaks mDNS over IPv6: a query sent to ff02::fb must be
# answered, the service PTR resolving to its instance and the SRV to the
# shared host.  Regression test for issue #10.

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

topo basic
mdnsd
browse6

OK
