lib_LTLIBRARIES      = libmdnsd.la

libmdnsd_la_SOURCES  = mdnsd.c mdnsd.h log.c 1035.c 1035.h sdtxt.c sdtxt.h xht.c xht.h
libmdnsd_la_CFLAGS   = -std=gnu99 -W -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
libmdnsd_la_CPPFLAGS = -D_GNU_SOURCE -D_BSD_SOURCE -D_DEFAULT_SOURCE
libmdnsd_la_LDFLAGS  = $(AM_LDFLAGS) -version-info 1:0:0
