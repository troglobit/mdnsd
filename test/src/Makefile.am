AUTOMAKE_OPTIONS        = subdir-objects

# Add source of new files to be in tarball here
EXTRA_DIST              = unittest.h xht.c addr_test.c

# Add new test programs here
noinst_PROGRAMS         = xht addr

addr_SOURCES            = addr_test.c

LIBS                   += ../../libmdnsd/libmdnsd.la $(cmocka_LIBS)

addr_LDADD              = ../../src/addr.o $(LIBOBJS)
addr_LDFLAGS            = $(AM_LDFLAGS) -Wl,--wrap=getifaddrs -Wl,--wrap=freeifaddrs

TESTS                   = xht
TESTS                  += addr
