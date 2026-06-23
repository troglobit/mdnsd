ACLOCAL_AMFLAGS         = -I m4

nobase_include_HEADERS  = libmdnsd/mdnsd.h libmdnsd/1035.h libmdnsd/sdtxt.h libmdnsd/xht.h
SUBDIRS                 = examples libmdnsd src
doc_DATA                = README.md ChangeLog.md LICENSE
EXTRA_DIST              = README.md ChangeLog.md LICENSE

if HAVE_SYSTEMD
systemd_DATA            = mdnsd.service
endif

if DOC
SUBDIRS                += man
endif
if ENABLE_TESTS
SUBDIRS                += test
endif

#
# Target to run when building a release
#
release: distcheck
	@mv $(DIST_ARCHIVES) ../

# Workaround for systemd unit file duing distcheck
DISTCHECK_CONFIGURE_FLAGS = --with-systemd=$$dc_install_base/$(systemd) --disable-silent-rules --enable-tests
DISTCLEANFILES = lib/.libs/*
