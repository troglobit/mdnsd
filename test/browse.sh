#!/bin/sh
# Verify RFC 6763 §4.1 browseability: the service-type PTR must point to
# the service instance name, so a browser can follow it to the SRV/TXT
# records.  Regression test for issue #80.

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

topo basic
mdnsd
browse

OK
