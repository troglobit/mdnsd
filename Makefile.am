ACLOCAL_AMFLAGS         = -I m4

sbin_PROGRAMS           = mdnsd
bin_PROGRAMS            = mquery
mdnsd_SOURCES           = mdnsd.c
mdnsd_LDADD             = libmdnsd/libmdnsd.la
mquery_SOURCES          = mquery.c
mquery_LDADD            = libmdnsd/libmdnsd.la

nobase_include_HEADERS  = libmdnsd/mdnsd.h libmdnsd/1035.h libmdnsd/sdtxt.h libmdnsd/xht.h
SUBDIRS                 = libmdnsd
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
