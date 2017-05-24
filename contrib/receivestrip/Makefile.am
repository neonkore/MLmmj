#

AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -g -Wall -pedantic -Wsign-compare -DDEFAULTTEXTDIR='"@textlibdir@"' -I$(srcdir)/../../include

if WANT_RECEIVESTRIP
  bin_PROGRAMS = mlmmj-receive-strip
endif

MLMMJ_SOURCES = mlmmj_src/mygetline.c mlmmj_src/memory.c mlmmj_src/readn.c  \
		mlmmj_src/strgen.c mlmmj_src/random-int.c mlmmj_src/log_error.c \
		mlmmj_src/print-version.c mlmmj_src/writen.c mlmmj_src/dumpfd2fd.c \
		mlmmj_src/ctrlvalues.c mlmmj_src/chomp.c

mlmmj_receive_strip_SOURCES = mlmmj-receive-strip.c $(MLMMJ_SOURCES)

mlmmj_src/%.c:
	ln -s ../../../src/$*.c $@
