## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign dist-bzip2
EXTRA_DIST = include VERSION LICENSE UPGRADE listtexts src/log_error.c FAQ \
	     TUNABLES
CLEANFILES = *~ mlmmj-*.tar.*

dist-hook:
	rm -rf `find $(distdir)/ -name CVS`

SUBDIRS = src
