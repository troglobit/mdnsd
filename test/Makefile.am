# Shell (integration) and cmocka (unit) tests share one Automake harness
# here so `make check TESTS=<name>` works for both kinds; see issue #66.
# subdir-objects lets the unit tests link $(LIBOBJS) from outside lib/.
AUTOMAKE_OPTIONS   = subdir-objects
AM_CPPFLAGS        = -I$(top_srcdir)

# Not covered by any _SOURCES, so ship these explicitly (the *.c unit
# tests are distributed automatically via _SOURCES).
EXTRA_DIST         = README.md lib.sh discover.sh iprecords.sh lostif.sh unittest.h
CLEANFILES         = *~ *.trs *.log

# top_srcdir is only needed for `make distcheck` (VPATH builds).
TESTS_ENVIRONMENT  = top_srcdir=$(top_srcdir)
TEST_EXTENSIONS    = .sh

# Only the shell integration tests need a mount+user namespace (to create
# network namespaces); the cmocka unit tests run directly.
SH_LOG_COMPILER    = unshare
AM_SH_LOG_FLAGS    = -mrun --map-auto

TESTS              = discover.sh
TESTS             += iprecords.sh
TESTS             += lostif.sh

if ENABLE_UNIT_TESTS
check_PROGRAMS     = xht addr answer
TESTS             += xht
TESTS             += addr
TESTS             += answer

xht_SOURCES        = xht.c
xht_LDADD          = ../libmdnsd/libmdnsd.la $(cmocka_LIBS) $(LIBOBJS)

addr_SOURCES       = addr_test.c
addr_LDADD         = ../libmdnsd/libmdnsd.la $(cmocka_LIBS) $(LIBOBJS) ../src/addr.o
# Link libmdnsd statically so --wrap intercepts the getifaddrs() call
# inside the library too, not just the copy in ../src/addr.o.
addr_LDFLAGS       = -static -Wl,--wrap=getifaddrs -Wl,--wrap=freeifaddrs

# answer_test.c #includes mdnsd.c to reach the static _a_copy(), so it
# compiles the library sources here rather than linking libmdnsd.la.
answer_SOURCES     = answer_test.c ../libmdnsd/1035.c ../libmdnsd/xht.c \
                     ../libmdnsd/sdtxt.c ../libmdnsd/log.c
answer_CPPFLAGS    = $(AM_CPPFLAGS)
answer_LDADD       = $(cmocka_LIBS) $(LIBOBJS)
endif
