#

AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -g -Wall -pedantic -Wsign-compare -DDEFAULTTEXTDIR='"@textlibdir@"'
INCLUDES = -I$(srcdir)/../../include

if WANT_RECEIVESTRIP
  bin_PROGRAMS = mlmmj-receive-strip
endif

mlmmj_receive_strip_SOURCES = mlmmj-receive-strip.c ../../src/mygetline.c ../../src/memory.c ../../src/readn.c  \
				../../src/strgen.c ../../src/random-int.c  ../../src/log_error.c ../../src/print-version.c \
				../../src/writen.c ../../src/dumpfd2fd.c ../../src/ctrlvalues.c ../../src/chomp.c
