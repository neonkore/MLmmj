#

AUTOMAKE_OPTIONS = foreign

CFLAGS = -g -Wall -pedantic -Wsign-compare
INCLUDES = -I../include

bin_PROGRAMS = mlmmj-send mlmmj-recieve mlmmj-process mlmmj-sub \
               mlmmj-unsub mlmmj-bounce mlmmj-maintd

bin_SCRIPTS = mlmmj-make-ml.sh

EXTRA_DIST = mlmmj-make-ml.sh

mlmmj_send_SOURCES = mlmmj-send.c writen.c mail-functions.c itoa.c chomp.c \
                     incindexfile.c checkwait_smtpreply.c getlistaddr.c \
		     mylocking.c init_sockfd.c strgen.c random-int.c \
		     print-version.c log_error.c mygetline.c memory.c \
		     statctrl.c ctrlvalue.c

mlmmj_recieve_SOURCES = mlmmj-recieve.c writen.c random-int.c strgen.c \
			print-version.c log_error.c dumpfd2fd.c memory.c

mlmmj_process_SOURCES = mlmmj-process.c writen.c find_email_adr.c \
			incindexfile.c itoa.c getlistaddr.c chomp.c \
			mylocking.c listcontrol.c random-int.c strgen.c \
			print-version.c send_help.c prepstdreply.c \
			do_all_the_voodo_here.c mygetline.c gethdrline.c \
			log_error.c statctrl.c ctrlvalue.c dumpfd2fd.c \
			subscriberfuncs.c ctrlvalues.c memory.c

mlmmj_sub_SOURCES = mlmmj-sub.c writen.c mylocking.c \
			getlistaddr.c chomp.c random-int.c strgen.c \
			subscriberfuncs.c print-version.c \
			log_error.c mygetline.c prepstdreply.c memory.c

mlmmj_unsub_SOURCES = mlmmj-unsub.c writen.c mylocking.c \
			getlistaddr.c chomp.c subscriberfuncs.c random-int.c \
			strgen.c print-version.c log_error.c mygetline.c \
			prepstdreply.c memory.c
			
mlmmj_bounce_SOURCES = mlmmj-bounce.c print-version.c log_error.c \
		       subscriberfuncs.c strgen.c random-int.c writen.c \
		       prepstdreply.c mygetline.c chomp.c getlistaddr.c \
		       memory.c

mlmmj_maintd_SOURCES = mlmmj-maintd.c print-version.c log_error.c mygetline.c \
		       strgen.c random-int.c chomp.c writen.c memory.c
