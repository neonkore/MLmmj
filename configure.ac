# Process this file with autoconf to produce a configure script.
AC_INIT(mlmmj, esyscmd([tr -d '\n' < VERSION]), mmj@mmj.dk)

VERSION=$PACKAGE_VERSION
AC_SUBST(VERSION)

AM_INIT_AUTOMAKE(mlmmj, $VERSION)
AC_CONFIG_SRCDIR([src/mlmmj-receive.c])
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB(socket,socket)
AC_CHECK_LIB(nsl,gethostbyname)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stddef.h stdlib.h string.h])
AC_CHECK_HEADERS([sys/socket.h syslog.h unistd.h time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

savex=$exec_prefix
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix=$prefix
tmp=$datadir/$PACKAGE/text.skel
while textlibdir=`eval echo "$tmp"`; test "x$textlibdir" != "x$tmp"; do tmp=$textlibdir; done
exec_prefix=$savex

AC_SUBST(textlibdir)

AC_ARG_ENABLE([receive-strip],
  AS_HELP_STRING([--enable-receive-strip],
                 [build mlmmj-receive-strip (default is no)]))
AM_CONDITIONAL(WANT_RECEIVESTRIP, test x"$enable_receive_strip" = xyes)

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([ftruncate memset socket strerror strncasecmp snprintf fcntl])
AC_CHECK_FUNCS([nanosleep time strftime syslog regcomp regexec])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([src/mlmmj-make-ml.sh])
AC_CONFIG_FILES([listtexts/Makefile])
AC_CONFIG_FILES([contrib/Makefile])
AC_CONFIG_FILES([contrib/receivestrip/Makefile])
AC_OUTPUT
