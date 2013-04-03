AC_PREREQ(2.57)
AC_INIT(highlander, 0.9.93, bjorn.augestad@gmail.com)
AC_CONFIG_AUX_DIR([extras])
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src/internals.h])
# AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_RANLIB
AC_PROG_CC
AC_PROG_CC_C_O
AM_PROG_CC_C_O

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h sys/socket.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([strerror_r inet_ntop localtime_r socket strerror strtol sendfile])

# With or without threads
#AC_ARG_ENABLE(tiny, 
#[ --enable-tiny, Enable Tiny build],
#[case "${enableval}" in
	#yes) tiny=true ;;
	#no)  tiny=false ;;
	#*) AC_MSG_ERROR(bad value ${enableval} for --enable-tiny) ;;
	#esac],[tiny=false])
#AM_CONDITIONAL(TINY, test x$tiny = xtrue)

AC_ARG_ENABLE([valgrind], 
	[--enable-valgrind Enable valgrind integration],
	[case ${enableval} in
	 	yes) valgrind=true;;
		no) valgrind=false;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-valgrind]) ;;
	esac],[valgrind=false])
AM_CONDITIONAL([WITH_VALGRIND], [test x$valgrind = xtrue])

AC_ARG_ENABLE([webcache], 
	[--enable-webcache Build webcache daemon],
	[case ${enableval} in
	 	yes) webcache=true;;
		no) webcache=false;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-webcache]) ;;
	esac],[webcache=false])
AM_CONDITIONAL([WITH_WEBCACHE], [test x$webcache = xtrue])

AC_ARG_ENABLE([utils], 
	[--enable-utils Build utility programs hipp and bin2c],
	[case ${enableval} in
	 	yes) utils=true;;
		no) utils=false;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-utils]) ;;
	esac],[utils=false])
AM_CONDITIONAL([WITH_UTILS], [test x$utils = xtrue])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT

