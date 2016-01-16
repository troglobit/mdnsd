ACLOCAL_AMFLAGS         = -I m4

lib_LTLIBRARIES         = libmdnsd.la
include_HEADERS         = mdnsd.h 1035.h sdtxt.h xht.h
libmdnsd_la_SOURCES     = mdnsd.c mdnsd.h 1035.c 1035.h sdtxt.c sdtxt.h xht.c xht.h
libmdnsd_la_CFLAGS      = -W -Wall -Wextra
libmdnsd_la_LDFLAGS     = $(AM_LDFLAGS) -version-info 0:0:0

sbin_PROGRAMS           = mhttp
bin_PROGRAMS            = mquery
mhttp_SOURCES           = mhttp.c
mhttp_LDADD             = libmdnsd.la
mquery_SOURCES          = mquery.c
mquery_LDADD            = libmdnsd.la

doc_DATA                = README.md LICENSE
EXTRA_DIST              = README.md LICENSE

## Generate MD5 checksum file
MD5 = md5sum
md5-dist:
	@for file in $(DIST_ARCHIVES); do	\
		$(MD5) $$file > $$file.md5;	\
	done

## Check if tagged in git
release-hook:
	if [ ! `git tag | grep $(PACKAGE_VERSION)` ]; then				\
		echo;									\
		printf "\e[1m\e[41mCannot find release tag $(PACKAGE_VERSION)\e[0m\n";	\
		printf "\e[1m\e[5mDo release anyway?\e[0m "; read yorn;			\
		if [ "$$yorn" != "y" -a "$$yorn" != "Y" ]; then				\
			printf "OK, aborting release.\n";				\
			exit 1;								\
		fi;									\
		echo;									\
	else										\
		echo;									\
		printf "\e[1m\e[42mFound GIT release tag $(PACKAGE_VERSION)\e[0m\n";	\
		printf "\e[1m\e[44m>>Remember to push tags!\e[0m\n";			\
		echo;									\
	fi

## Target to run when building a release
release: dist release-hook md5-dist
	@for file in $(DIST_ARCHIVES); do						\
		printf "$$file    \tDistribution tarball\n";				\
		printf "$$file.md5\t"; cat $$file.md5 | cut -f1 -d' ';			\
		mv $$file* ../;								\
	done
