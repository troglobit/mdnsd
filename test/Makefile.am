EXTRA_DIST         = lib.sh discover.sh iprecords.sh
CLEANFILES         = *~ *.trs *.log
SUBDIRS            = src

TEST_EXTENSIONS    = .sh
TESTS_ENVIRONMENT  = unshare -mrun

TESTS              = discover.sh
TESTS             += iprecords.sh
