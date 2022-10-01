AUTOMAKE_OPTIONS        = subdir-objects
AM_CPPFLAGS             = -I$(top_srcdir)
AM_LDADD                = ../../libmdnsd/libmdnsd.la $(cmocka_LIBS) $(LIBOBJS)

# Add source of new files to be in tarball here
EXTRA_DIST              = unittest.h xht.c addr_test.c

# Add new test programs here
noinst_PROGRAMS         = addr xht

addr_SOURCES            = addr_test.c
addr_CPPFLAGS           = $(AM_CPPFLAGS)
addr_LDADD              = $(AM_LDADD) ../../src/addr.o
addr_LDFLAGS            = $(AM_LDFLAGS) -Wl,--wrap=getifaddrs -Wl,--wrap=freeifaddrs

xht_SOURCES             = xht.c
xht_CPPFLAGS            = $(AM_CPPFLAGS)
xht_LDADD               = $(AM_LDADD)
xht_LDFLAGS             = $(AM_LDFLAGS)

TESTS                   = xht
TESTS                  += addr
