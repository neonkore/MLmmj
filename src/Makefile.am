#

AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -g -Wall -pedantic -Wsign-compare -DDEFAULTTEXTDIR='"@textlibdir@"' -I$(srcdir)/../include

bin_PROGRAMS = mlmmj-send mlmmj-receive mlmmj-process mlmmj-sub \
               mlmmj-unsub mlmmj-bounce mlmmj-maintd mlmmj-list

bin_SCRIPTS = mlmmj-make-ml

EXTRA_DIST = mlmmj-make-ml

mlmmj_send_SOURCES = mlmmj-send.c writen.c mail-functions.c itoa.c chomp.c \
                     incindexfile.c checkwait_smtpreply.c getlistaddr.c \
		     mylocking.c init_sockfd.c strgen.c random-int.c \
		     print-version.c log_error.c mygetline.c memory.c \
		     statctrl.c ctrlvalue.c getaddrsfromfd.c readn.c \
		     getlistdelim.c

mlmmj_receive_SOURCES = mlmmj-receive.c writen.c random-int.c strgen.c \
			print-version.c log_error.c dumpfd2fd.c memory.c \
			log_oper.c mylocking.c readn.c

mlmmj_process_SOURCES = mlmmj-process.c writen.c find_email_adr.c \
			incindexfile.c itoa.c getlistaddr.c chomp.c \
			mylocking.c listcontrol.c random-int.c strgen.c \
			print-version.c send_help.c prepstdreply.c \
			do_all_the_voodoo_here.c mygetline.c gethdrline.c \
			log_error.c statctrl.c ctrlvalue.c dumpfd2fd.c \
			subscriberfuncs.c ctrlvalues.c memory.c log_oper.c \
			send_list.c readn.c getlistdelim.c unistr.c

mlmmj_sub_SOURCES = mlmmj-sub.c writen.c mylocking.c \
			getlistaddr.c chomp.c random-int.c strgen.c \
			subscriberfuncs.c print-version.c \
			log_error.c mygetline.c prepstdreply.c memory.c \
			statctrl.c readn.c getlistdelim.c ctrlvalues.c \
			unistr.c ctrlvalue.c

mlmmj_unsub_SOURCES = mlmmj-unsub.c writen.c mylocking.c \
			getlistaddr.c chomp.c subscriberfuncs.c random-int.c \
			strgen.c print-version.c log_error.c mygetline.c \
			prepstdreply.c memory.c statctrl.c readn.c \
			getlistdelim.c unistr.c ctrlvalue.c
			
mlmmj_bounce_SOURCES = mlmmj-bounce.c print-version.c log_error.c \
		       subscriberfuncs.c strgen.c random-int.c writen.c \
		       prepstdreply.c mygetline.c chomp.c getlistaddr.c \
		       memory.c find_email_adr.c gethdrline.c readn.c \
		       getlistdelim.c unistr.c ctrlvalue.c statctrl.c

mlmmj_maintd_SOURCES = mlmmj-maintd.c print-version.c log_error.c mygetline.c \
		       strgen.c random-int.c chomp.c writen.c memory.c \
		       ctrlvalue.c send_digest.c getlistaddr.c dumpfd2fd.c \
		       mylocking.c log_oper.c readn.c getlistdelim.c \
 		       prepstdreply.c statctrl.c gethdrline.c unistr.c

mlmmj_list_SOURCES = mlmmj-list.c strgen.c writen.c print-version.c memory.c \
		     log_error.c random-int.c readn.c subscriberfuncs.c

install-exec-hook:
	ln -f -s mlmmj-receive $(DESTDIR)$(bindir)/mlmmj-recieve
	ln -f -s mlmmj-make-ml $(DESTDIR)$(bindir)/mlmmj-make-ml.sh
