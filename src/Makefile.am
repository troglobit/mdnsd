AM_CFLAGS               = -std=gnu99 -W -Wall -Wextra -Wno-unused-parameter
AM_CPPFLAGS             = -D_GNU_SOURCE -D_BSD_SOURCE -D_DEFAULT_SOURCE -I$(top_srcdir)

sbin_PROGRAMS           = mdnsd
bin_PROGRAMS            = mquery

mdnsd_SOURCES           = mdnsd.c mdnsd.h addr.c conf.c
mdnsd_LDADD             = ../libmdnsd/libmdnsd.la

mquery_SOURCES          = mquery.c
mquery_LDADD            = ../libmdnsd/libmdnsd.la
