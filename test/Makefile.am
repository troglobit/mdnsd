EXTRA_DIST         = lib.sh discover.sh iprecords.sh lostif.sh
CLEANFILES         = *~ *.trs *.log
if ENABLE_UNIT_TESTS
SUBDIRS            = src
endif

# This env. variable is only needed for `make distcheck`
TESTS_ENVIRONMENT  = top_srcdir=$(top_srcdir) unshare -mrun
TEST_EXTENSIONS    = .sh

TESTS              = discover.sh
TESTS             += iprecords.sh
TESTS             += lostif.sh
