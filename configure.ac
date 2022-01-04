# Use v2.61 for backwards compat with Ubuntu 12.04 LTS
AC_PREREQ([2.61])
AC_INIT(mdnsd, 0.11-dev, https://github.com/troglobit/mdnsd/issues)
AC_CONFIG_AUX_DIR(aux)
AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR(src/mdnsd.c)
AM_CONFIG_HEADER(config.h)
AC_CONFIG_FILES([Makefile mdnsd.service examples/Makefile libmdnsd/Makefile man/Makefile src/Makefile])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_AR
LT_INIT
PKG_PROG_PKG_CONFIG

AC_HEADER_STDC
AC_C_CONST
 
# Check for usually missing API's, which we can replace
AC_REPLACE_FUNCS([pidfile strlcpy utimensat])
AC_CONFIG_LIBOBJ_DIR([lib])

AC_CHECK_MEMBERS([struct ip_mreqn.imr_ifindex], , ,
[
#include <netinet/in.h>
])

# Options
AC_ARG_WITH([systemd],
	[AS_HELP_STRING([--with-systemd=DIR], [Directory for systemd service files])],,
	[with_systemd=auto])

# Check where to install the systemd .service file
AS_IF([test "x$with_systemd" = "xyes" -o "x$with_systemd" = "xauto"], [
	def_systemd=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
	AS_IF([test "x$def_systemd" = "x"],
		[AS_IF([test "x$with_systemd" = "xyes"],
			[AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
			with_systemd=no], [with_systemd="$def_systemd"])])
AS_IF([test "x$with_systemd" != "xno"],
	[AC_SUBST([systemddir], [$with_systemd])])
AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemd" != "xno"])

# Expand $sbindir early, into $SBINDIR, for systemd unit file
# NOTE: This does *not* take prefix/exec_prefix override at "make
#       install" into account, unfortunately.
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
SYSCONFDIR=`eval echo $sysconfdir`
SYSCONFDIR=`eval echo $SYSCONFDIR`
AC_SUBST(SYSCONFDIR)
BINDIR=`eval echo $bindir`
BINDIR=`eval echo $BINDIR`
AC_SUBST(BINDIR)
SBINDIR=`eval echo $sbindir`
SBINDIR=`eval echo $SBINDIR`
AC_SUBST(SBINDIR)

# Workaround for autoconf < 2.70, although some major distros have
# backported support for runstatedir already.
AS_IF([test -z "$runstatedir"], runstatedir="$localstatedir/run")
AC_SUBST(runstatedir)

# Create all config files
AC_OUTPUT

# Expand directories for configuration summary, unexpanded defaults:
# sysconfdir  => ${prefix}/etc
# runstatedir => ${localstatedir}/run
SYSCONFDIR=`eval echo $sysconfdir`
LOCALSTATEDIR=`eval echo $localstatedir`
RUNSTATEDIR=`eval echo $runstatedir`
RUNSTATEDIR=`eval echo $RUNSTATEDIR`

cat <<EOF

------------------ Summary ------------------
 $PACKAGE_NAME version $PACKAGE_VERSION
  Install prefix.....: $prefix
  mdnsd install dir..: $SBINDIR
  mquery install dir.: $BINDIR
  PID file...........: $RUNSTATEDIR/mdnsd.pid
  mDNS-SD .service in: $SYSCONFDIR/mdns.d/*.service
  C Compiler.........: $CC $CFLAGS $CPPFLAGS $LDFLAGS $LIBS

 Optional features:
  systemd............: $with_systemd

------------- Compiler version --------------
$($CC --version || true)
---------------------------------------------

Check the above options and compile with:
 ${MAKE-make}

EOF
