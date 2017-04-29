AC_PREREQ(2.61)
AC_INIT(highlander, 0.9.98, bjorn@augestad.online)
AC_CONFIG_AUX_DIR([extras])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
AM_EXTRA_RECURSIVE_TARGETS([foo])


AC_CONFIG_SRCDIR([http/src/internals.h])
# AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_RANLIB
AM_PROG_CC_C_O

# Checks for header files.
#AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h sys/socket.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_MODE_T
AC_TYPE_PID_T

# Checks for library functions.
#AC_CHECK_FUNCS([strerror_r inet_ntop localtime_r socket strerror strtol sendfile])

AC_ARG_ENABLE([valgrind], 
	[--enable-valgrind Enable valgrind integration],
	[case ${enableval} in
	 	yes) valgrind=true;;
		no) valgrind=false;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-valgrind]) ;;
	esac],[valgrind=false])
AM_CONDITIONAL([WITH_VALGRIND], [test x$valgrind = xtrue])

AC_CONFIG_FILES([
	Makefile
	tools/Makefile
	meta/Makefile
	meta/src/Makefile
	meta/doc/Makefile
	http/Makefile
	http/src/Makefile
	http/doc/Makefile
	apps/Makefile
    apps/webcache/Makefile apps/ftp/Makefile apps/helloworld/Makefile
	apps/echoserver/Makefile
	apps/webclient/Makefile
    ])
AC_OUTPUT

