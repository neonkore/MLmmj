# Process this file with autoconf to produce a configure script.
AC_INIT(mlmmj, esyscmd([tr -d '\n' < VERSION]), mmj@mmj.dk)

VERSION=$(cat VERSION)
AC_SUBST(VERSION)

AM_INIT_AUTOMAKE(mlmmj, $VERSION)
AC_CONFIG_SRCDIR([src/mlmmj-recieve.c])
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB(socket,socket)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stddef.h stdlib.h string.h])
AC_CHECK_HEADERS([sys/socket.h syslog.h unistd.h time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([ftruncate memset socket strerror strncasecmp snprintf fcntl])
AC_CHECK_FUNCS([nanosleep time strftime syslog regcomp regexec])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_OUTPUT
