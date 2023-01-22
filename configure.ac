# Use v2.61 for backwards compat with Ubuntu 12.04 LTS
AC_PREREQ([2.61])
AC_INIT([mdnsd],[0.12-dev],[https://github.com/troglobit/mdnsd/issues])
AC_CONFIG_AUX_DIR(aux)
AM_INIT_AUTOMAKE([1.11 foreign dist-xz])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR(src/mdnsd.c)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([Makefile mdnsd.service
		 examples/Makefile
		 libmdnsd/Makefile
		 man/Makefile
		 src/Makefile
		 test/Makefile
		 test/src/Makefile])
AC_CONFIG_MACRO_DIR([m4])

# Define necessary build flags
AC_USE_SYSTEM_EXTENSIONS

AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_AR
LT_INIT
PKG_PROG_PKG_CONFIG

# Check for usually missing API's, which we can replace
AC_REPLACE_FUNCS([pidfile strlcpy utimensat])
AC_CONFIG_LIBOBJ_DIR([lib])

AC_CHECK_HEADERS([net/if.h sys/param.h sys/socket.h netinet/in.h])
AC_CHECK_MEMBERS([struct ip_mreqn.imr_ifindex], [], [], [
        #ifdef HAVE_NETINET_IN_H
        # include <netinet/in.h>
        #endif
])

AC_MSG_CHECKING(for SO_BINDTODEVICE in net/if.h)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
	#include <net/if.h>
], [
	int i = SO_BINDTODEVICE;
])],[ac_cv_have_so_bindtodevice=yes], [ac_cv_have_so_bindtodevice=no])
AS_IF([test "x$ac_cv_have_so_bindtodevice" = "xyes"],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_SO_BINDTODEVICE, 1, [Whether our sockets support SO_BINDTODEVICE])],[
	AC_MSG_RESULT(no)])

# Solaris/Illumos has networking functions in a separate library
AC_CHECK_LIB(socket, bind)

# Options
AC_ARG_ENABLE(tests,
        [AS_HELP_STRING([--enable-tests], [Enable tests, requires cmocka library])],
        [enable_tests="$enableval"],
        [enable_tests="no"])

AC_ARG_ENABLE(doc,
        AS_HELP_STRING([--disable-doc], [Disable installing documentation]),,
	[enable_doc=yes])

AC_ARG_WITH([systemd],
	[AS_HELP_STRING([--with-systemd=DIR], [Directory for systemd service files])],,
	[with_systemd=auto])

AC_ARG_WITH(mquery,
     AS_HELP_STRING([--without-mquery], [Build without mdns-scan like mquery tool]),
     [mquery=$withval], [mquery='yes'])

AM_CONDITIONAL(DOC, [test "x$enable_doc" = "xyes"])

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

AS_IF([test "x$mquery" != "xno"], with_mquery="yes", with_mquery="no")
AM_CONDITIONAL([ENABLE_MQUERY], [test "x$with_mquery" != "xno"])

PKG_CHECK_EXISTS([cmocka >= 1.1.0], [cmocka_avail=yes], [cmocka_avail=no])
AS_IF([test "x$enable_tests" = "xyes" -a "x$cmocka_avail" = "xyes"], [
	PKG_CHECK_MODULES([cmocka], [cmocka >= 1.1.0])
	unit_tests=yes], [unit_tests=no])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" != "xno"])
AM_CONDITIONAL([ENABLE_UNIT_TESTS], [test "x$cmocka_avail" != "xno"])

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
  cmocka available...: $cmocka_avail

 Optional features:
  documentations.....: $enable_doc
  mquery.............: $with_mquery
  systemd............: $with_systemd
  test suite.........: $enable_tests
  unit tests.........: $unit_tests

------------- Compiler version --------------
$($CC --version || true)
---------------------------------------------

Check the above options and compile with:
 ${MAKE-make}

EOF
