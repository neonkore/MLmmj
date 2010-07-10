## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign dist-bzip2
EXTRA_DIST = include VERSION LICENSE UPGRADE src/log_error.c FAQ \
	     TUNABLES README.access man \
	     README.exim4 README.sendmail README.security README.qmail \
	     README.postfix
CLEANFILES = *~ mlmmj-*.tar.*

dist-hook:
	rm -rf `find $(distdir)/ -name CVS`

man1_MANS = man/mlmmj-bounce.1 man/mlmmj-make-ml.sh.1 man/mlmmj-recieve.1 \
	    man/mlmmj-sub.1 man/mlmmj-maintd.1 man/mlmmj-process.1 \
	    man/mlmmj-send.1 man/mlmmj-unsub.1 man/mlmmj-list.1

SUBDIRS = src listtexts contrib
