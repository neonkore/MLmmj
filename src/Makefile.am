#

AUTOMAKE_OPTIONS = foreign

CFLAGS = -g -O2 -Wall -pedantic -Wsign-compare
INCLUDES = -I../include

bin_PROGRAMS = mlmmj-send mlmmj-recieve mlmmj-process mlmmj-sub \
               mlmmj-unsub

bin_SCRIPTS = mlmmj-make-ml.sh

EXTRA_DIST = mlmmj-make-ml.sh

mlmmj_send_SOURCES = mlmmj-send.c writen.c mail-functions.c itoa.c chomp.c \
                     incindexfile.c checkwait_smtpreply.c getlistaddr.c \
		     mylocking.c init_sockfd.c strgen.c random-int.c \
		     print-version.c log_error.c

mlmmj_recieve_SOURCES = mlmmj-recieve.c writen.c random-int.c header_token.c \
			getlistaddr.c chomp.c strgen.c print-version.c \
			mygetline.c log_error.c

mlmmj_process_SOURCES = mlmmj-process.c writen.c find_email_adr.c \
			incindexfile.c itoa.c getlistaddr.c chomp.c \
			mylocking.c listcontrol.c random-int.c strgen.c \
			header_token.c print-version.c send_help.c \
			do_all_the_voodo_here.c mygetline.c gethdrline.c \
			log_error.c

mlmmj_sub_SOURCES = mlmmj-sub.c writen.c mylocking.c \
			getlistaddr.c chomp.c random-int.c strgen.c \
			subscriberfuncs.c print-version.c \
			log_error.c mygetline.c

mlmmj_unsub_SOURCES = mlmmj-unsub.c writen.c mylocking.c \
			getlistaddr.c chomp.c subscriberfuncs.c random-int.c \
			strgen.c print-version.c log_error.c mygetline.c
