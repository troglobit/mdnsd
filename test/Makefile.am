EXTRA_DIST         = lib.sh discover.sh iprecords.sh
CLEANFILES         = *~ *.trs *.log
SUBDIRS            = src

# This env. variable is only needed for `make distcheck`
TESTS_ENVIRONMENT  = top_srcdir=$(top_srcdir) unshare -mrun
TEST_EXTENSIONS    = .sh

TESTS              = discover.sh
TESTS             += iprecords.sh
