AC_PREREQ(2.61)
AC_INIT(highlander, 0.9.99, bjorn.augestad@gmail.com)
AC_CONFIG_AUX_DIR([extras])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
AM_EXTRA_RECURSIVE_TARGETS([foo])

dnl Install to $HOME by default (Qubes-friendly)
AC_PREFIX_DEFAULT([${HOME}])

AC_CONFIG_SRCDIR([http/src/internals.h])
dnl AC_CONFIG_HEADER([config.h])

dnl --- Tools ---
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_RANLIB
AM_PROG_CC_C_O

dnl --- Headers / types ---
AC_CHECK_HEADERS([openssl/ssl.h arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h sys/socket.h syslog.h unistd.h])
AC_TYPE_MODE_T
AC_TYPE_PID_T

dnl --- Options ---
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--disable-debug],[release build (-O3 -DNDEBUG), default is debug])],
  [],[enable_debug=yes])

AC_ARG_ENABLE([tsan],
  [AS_HELP_STRING([--enable-tsan],[build with ThreadSanitizer (no ASan/UBSan/LSan)])],
  [],[enable_tsan=no])

AS_IF([test "x$enable_tsan" = "xyes" && test "x$enable_debug" = "xyes"],[
  AC_MSG_WARN([--enable-tsan overrides debug sanitizers])
])

dnl --- Hardening commons ---
HARDEN_CFLAGS="-fPIE -fstack-protector-strong -D_FORTIFY_SOURCE=3 -fstack-clash-protection -fcf-protection=full"
HARDEN_LDFLAGS="-pie -Wl,-z,relro,-z,now -Wl,--as-needed"

dnl --- Flag sets ---
CFLAGS_DEBUG_BASE="-std=gnu2x -pthread -I\$(srcdir) -I\$(top_srcdir)/src \
 -Werror -Wshadow -W -Wmissing-prototypes \
 -Wformat-overflow -Woverflow -Wshift-overflow \
 -Og -ggdb -fno-omit-frame-pointer \
 -Wextra -Wall -pedantic -Wno-long-long -Wno-inline \
 $HARDEN_CFLAGS"

CFLAGS_ASANUB="$CFLAGS_DEBUG_BASE -fsanitize=address,undefined,leak"
LDFLAGS_ASANUB="$HARDEN_LDFLAGS"
SANLIBS_ASANUB="-Wl,--no-as-needed -fsanitize=address,undefined,leak -Wl,--as-needed"

CFLAGS_TSAN="$CFLAGS_DEBUG_BASE -fsanitize=thread"
LDFLAGS_TSAN="$HARDEN_LDFLAGS -pthread"
SANLIBS_TSAN="-Wl,--no-as-needed -fsanitize=thread -Wl,--as-needed"

CFLAGS_RELEASE="-std=gnu2x -pthread -I\$(srcdir) -I\$(top_srcdir)/src \
 -Werror -Wshadow -W -Wmissing-prototypes \
 -Wformat-overflow -Woverflow -Wshift-overflow \
 -O3 -DNDEBUG -Wextra -Wall -pedantic -Wno-long-long -Wno-inline \
 $HARDEN_CFLAGS"

LDFLAGS_RELEASE="$HARDEN_LDFLAGS"

dnl --- Choose profile ---
AS_IF([test "x$enable_tsan" = "xyes"],[
  AC_MSG_NOTICE([Building with ThreadSanitizer])
  CFLAGS="$CFLAGS_TSAN"
  LDFLAGS="$LDFLAGS_TSAN"
  LIBS="$LIBS $SANLIBS_TSAN"      dnl lands last in link order
  AC_DEFINE([TSAN_BUILD],[1],[ThreadSanitizer build])
],[
  AS_IF([test "x$enable_debug" = "xyes"],[
    AC_MSG_NOTICE([Building with ASan/UBSan/LSan])
    CFLAGS="$CFLAGS_ASANUB"
    LDFLAGS="$LDFLAGS_ASANUB"
    LIBS="$LIBS $SANLIBS_ASANUB"  dnl lands last
    AC_DEFINE([DEBUG_BUILD],[1],[Debug build])
  ],[
    AC_MSG_NOTICE([Building release])
    CFLAGS="$CFLAGS_RELEASE"
    LDFLAGS="$LDFLAGS_RELEASE"
  ])
])

dnl --- (Optional) expose flag sets if Makefiles want them
AC_SUBST([CFLAGS_DEBUG_BASE])
AC_SUBST([CFLAGS_ASANUB]) AC_SUBST([LDFLAGS_ASANUB]) AC_SUBST([SANLIBS_ASANUB])
AC_SUBST([CFLAGS_TSAN])   AC_SUBST([LDFLAGS_TSAN])   AC_SUBST([SANLIBS_TSAN])
AC_SUBST([CFLAGS_RELEASE]) AC_SUBST([LDFLAGS_RELEASE])

dnl --- Makefiles ---
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

