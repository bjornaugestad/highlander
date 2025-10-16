AC_PREREQ(2.61)
AC_INIT(highlander, 0.9.99, bjorn.augestad@gmail.com)
AC_CONFIG_AUX_DIR([extras])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
AM_EXTRA_RECURSIVE_TARGETS([foo])


# Set default destination(prefix) to home diretory so we 
# can install stuff persistently on Qubes
AC_PREFIX_DEFAULT([${HOME}])

AC_CONFIG_SRCDIR([http/src/internals.h])
# AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_RANLIB
AM_PROG_CC_C_O

# Checks for header files.
AC_CHECK_HEADERS([openssl/ssl.h arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h sys/socket.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_MODE_T
AC_TYPE_PID_T

# Checks for library functions.
#AC_CHECK_FUNCS([strerror_r inet_ntop localtime_r socket strerror strtol sendfile])

# default = debug
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--disable-debug],[build with -O3 -DNDEBUG instead of sanitizers])],
  [],[enable_debug=yes])

# hardening commons
HARDEN_CFLAGS="-fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=3 -fstack-clash-protection -fcf-protection=full"
HARDEN_LDFLAGS="-pie -Wl,-z,relro,-z,now -Wl,--as-needed"

AC_ARG_ENABLE([debug],[AS_HELP_STRING([--disable-debug],[release -O3 -DNDEBUG])],[],[enable_debug=yes])

CFLAGS_DEBUG="-std=gnu2x -pthread -I\$(srcdir) -I\$(top_srcdir)/src \
 -Werror -Wshadow -W -Wmissing-prototypes \
 -Wformat-overflow -Woverflow -Wshift-overflow  \
 -Og -ggdb -fno-omit-frame-pointer \
 -fsanitize=address,undefined,leak \
 -Wextra -Wall -pedantic -Wno-long-long -Wno-inline \
 $HARDEN_CFLAGS"

LDFLAGS_DEBUG="-fsanitize=address,undefined,leak $HARDEN_LDFLAGS"

CFLAGS_RELEASE="-std=gnu2x -pthread -I\$(srcdir) -I\$(top_srcdir)/src \
 -Werror -Wshadow -W -Wmissing-prototypes \
 -Wformat-overflow -Woverflow -Wshift-overflow  \
 -O3 -DNDEBUG -Wextra -Wall -pedantic -Wno-long-long -Wno-inline \
 $HARDEN_CFLAGS"

LDFLAGS_RELEASE="$HARDEN_LDFLAGS"

if test "x$enable_debug" = "xyes"; then
  CFLAGS="$CFLAGS_DEBUG"
  LDFLAGS="$LDFLAGS_DEBUG"
  AC_DEFINE([DEBUG_BUILD],[1],[Debug build])
else
  CFLAGS="$CFLAGS_RELEASE"
  LDFLAGS="$LDFLAGS_RELEASE"
fi

AC_SUBST([CFLAGS_DEBUG]) AC_SUBST([CFLAGS_RELEASE])
AC_SUBST([LDFLAGS_DEBUG]) AC_SUBST([LDFLAGS_RELEASE])

AC_CONFIG_FILES([
	Makefile
	tools/Makefile
    json/Makefile
	meta/Makefile
	meta/src/Makefile
	meta/doc/Makefile
	http/Makefile
	http/src/Makefile
	http/doc/Makefile
	imap/Makefile
	imap/src/Makefile
	apps/Makefile
    apps/webcache/Makefile
    apps/ftp/Makefile
    apps/helloworld/Makefile
	apps/httpd/Makefile
	apps/echoserver/Makefile
	apps/webclient/Makefile
    ])
AC_OUTPUT

