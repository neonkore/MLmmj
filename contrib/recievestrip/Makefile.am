#

AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -g -Wall -pedantic -Wsign-compare -DDEFAULTTEXTDIR='"@textlibdir@"'
INCLUDES = -I$(srcdir)/../../include

bin_PROGRAMS = mlmmj-recieve-strip

mlmmj_recieve_strip_SOURCES = mlmmj-recieve-strip.c ../../src/mygetline.c ../../src/memory.c ../../src/readn.c  \
				../../src/strgen.c ../../src/random-int.c  ../../src/log_error.c ../../src/print-version.c \
				../../src/writen.c ../../src/dumpfd2fd.c ../../src/ctrlvalues.c ../../src/chomp.c
