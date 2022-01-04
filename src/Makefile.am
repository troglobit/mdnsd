AUTOMAKE_OPTIONS        = subdir-objects
AM_CFLAGS               = -std=gnu99 -W -Wall -Wextra -Wno-unused-parameter
AM_CPPFLAGS             = -D_GNU_SOURCE -D_BSD_SOURCE -D_DEFAULT_SOURCE
AM_CPPFLAGS            += -I$(top_srcdir) -D_PIDFILEDIR=\"@runstatedir@\"

sbin_PROGRAMS           = mdnsd
bin_PROGRAMS            = mquery

mdnsd_SOURCES           = mdnsd.c mdnsd.h addr.c conf.c queue.h
mdnsd_LDADD             = ../libmdnsd/libmdnsd.la $(LIBS) $(LIBOBJS)

mquery_SOURCES          = mquery.c
mquery_LDADD            = ../libmdnsd/libmdnsd.la $(LIBS) $(LIBOBJS)
