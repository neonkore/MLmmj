/* Copyright (C) 2002, 2003, 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mlmmj-send.h"
#include "mlmmj.h"
#include "mail-functions.h"
#include "itoa.h"
#include "incindexfile.h"
#include "chomp.h"
#include "checkwait_smtpreply.h"
#include "getlistaddr.h"
#include "init_sockfd.h"
#include "strgen.h"
#include "log_error.h"


static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/chat-list -m /path/to/mail\n", prg);
	exit(EXIT_SUCCESS);
}

static char *bounce_from_adr(char *recipient, char *listadr, char *mailfilename)
{
	char *bounce_adr;
	char *indexstr, *listdomain, *a;
	size_t len;

	indexstr = strrchr(mailfilename, '/');
	if (indexstr) {
		indexstr++;  /* skip the slash */
	} else {
		indexstr = mailfilename;
	}

	recipient = strdup(recipient);
	if (!recipient) {
		return NULL;
	}
	a = strchr(recipient, '@');
	if (a) *a = '=';

	listadr = strdup(listadr);
	if (!listadr) {
		free(recipient);
		return NULL;
	}

	listdomain = strchr(listadr, '@');
	if (!listdomain) {
		free(recipient);
		free(listadr);
	}
	*listdomain++ = '\0';

	/* 12 = RECIPDELIM + "bounces-" + "-" + "@" + NUL */
	len = strlen(listadr) + strlen(recipient) + strlen(indexstr)
		 + strlen(listdomain) + 12;
	bounce_adr = malloc(len);
	if (!bounce_adr) {
		free(recipient);
		free(listadr);
		return NULL;
	}
	snprintf(bounce_adr, len, "%s%cbounces-%s-%s@%s", listadr, RECIPDELIM,
		 recipient, indexstr, listdomain);

	free(recipient);
	free(listadr);
	return bounce_adr;
}

int send_mail(int sockfd, const char *from, const char *to, const char *replyto,
		FILE *mailfile)
{
	int retval;

	if((retval = write_mail_from(sockfd, from)) != 0) {
		log_error(LOG_ARGS, "Could not write MAIL FROM\n");
		/* FIXME: Queue etc.*/
		write_rset(sockfd);
		return retval;
	}
	if((retval = checkwait_smtpreply(sockfd, MLMMJ_FROM)) != 0) {
		log_error(LOG_ARGS, "Wrong MAIL FROM:\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}

	if((retval = write_rcpt_to(sockfd, to)) != 0) {
		log_error(LOG_ARGS, "Could not write RCPT TO:\n");
		/* FIXME: Queue etc.*/
		write_rset(sockfd);
		return retval;
	}
	if((retval = checkwait_smtpreply(sockfd, MLMMJ_RCPTTO)) != 0) {
		log_error(LOG_ARGS, "Wrong RCPT TO:\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}
	if((retval = write_data(sockfd)) != 0) {
		log_error(LOG_ARGS, "Could not write DATA\b");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}
	if((checkwait_smtpreply(sockfd, MLMMJ_DATA)) != 0) {
		log_error(LOG_ARGS, "Mailserver not ready for DATA\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}
	if(replyto)
		if((retval = write_replyto(sockfd, replyto)) != 0) {
			log_error(LOG_ARGS, "Could not write reply-to addr.\n");
			write_rset(sockfd);
			/* FIXME: Queue etc.*/
			return retval;
		}
	if((retval = write_mailbody_from_file(sockfd, mailfile)) != 0) {
		log_error(LOG_ARGS, "Could not write mailbody\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}

	if((retval = write_dot(sockfd)) != 0) {
		log_error(LOG_ARGS, "Could not write <CR><LF>.<CR><LF>\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}

	if((checkwait_smtpreply(sockfd, MLMMJ_DOT)) != 0) {
		log_error(LOG_ARGS, "Mailserver did not acknowledge end of mail\n"
				"<CR><LF>.<CR><LF> was written, to no"
				"avail\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}

	return 0;
}

int main(int argc, char **argv)
{
	size_t len = 0;
	int sockfd = 0, opt, mindex, retval = 0;
	FILE *subfile = NULL, *mailfile = NULL;
	char *listadr, buf[READ_BUFSIZE];
	char *mailfilename = NULL, *subfilename = NULL, *listdir = NULL;
	char *replyto = NULL, *bounce_adr = NULL, *to_addr = NULL;
	char *bufres, *relayhost = NULL, *archivefilename = NULL;
	char *listctrl = NULL;
	int deletewhensent = 1;
	
	log_set_name(argv[0]);

	while ((opt = getopt(argc, argv, "VDhm:l:L:R:F:T:r:")) != -1){
		switch(opt) {
		case 'D':
			deletewhensent = 0;
			break;
		case 'F':
			bounce_adr = optarg;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'l':
			listctrl = optarg;
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'm':
			mailfilename = optarg;
			break;
		case 'r':
			relayhost = optarg;
			break;
		case 'R':
			replyto = optarg;
			break;
		case 'T':
			to_addr = optarg;
			break;
		case 'V':
			print_version(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	if(mailfilename == NULL || (listdir == NULL && listctrl == NULL)) {
		fprintf(stderr, "You have to specify -m and -L or -l\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(!listctrl && listdir && listdir[0] == '1')
		listctrl = strdup("1");

	if(!listctrl)
		listctrl = strdup("0");

	
	/* get the list address */
	if(listctrl[0] == '1' && (bounce_adr == NULL || to_addr == NULL)) {
		fprintf(stderr, "With -l 1 you need -F and -T\n");
		exit(EXIT_FAILURE);
	}

	if((listctrl[0] == '2' && listdir == NULL)) {
		fprintf(stderr, "With -l 2 you need -L\n");
		exit(EXIT_FAILURE);
	}

	if(listctrl[0] != '1' && listctrl[0] != '2')
		listadr = getlistaddr(listdir);
	
	/* initialize file with mail to send */

	if((mailfile = fopen(mailfilename, "r")) == NULL) {
	        log_error(LOG_ARGS, "Could not open '%s'", mailfilename);
		exit(EXIT_FAILURE);
	}

	if(relayhost)
		init_sockfd(&sockfd, relayhost);
	else
		init_sockfd(&sockfd, RELAYHOST);

	switch(listctrl[0]) {
	case '1': /* A single mail is to be sent, do nothing */
		break;
	case '2': /* Moderators */
		subfilename = concatstr(2, listdir, "/moderators");
		if((subfile = fopen(subfilename, "r")) == NULL) {
			log_error(LOG_ARGS, "Could not open '%s':",
					    subfilename);
			free(subfilename);
			/* No moderators is no error. Could be the sysadmin
			 * likes to do it manually.
			 */
			exit(EXIT_SUCCESS);
		}
		break;
	default: /* normal list mail */
		subfilename = concatstr(2, listdir, "/subscribers");
		if((subfile = fopen(subfilename, "r")) == NULL) {
			log_error(LOG_ARGS, "Could not open '%s':",
					    subfilename);
			free(subfilename);
			exit(EXIT_FAILURE);
		}
		break;
	}

	/* initialize the archive filename */
	if(listctrl[0] != '1' && listctrl[0] != '2') {
		mindex = incindexfile((const char *)listdir, 1);
		len = strlen(listdir) + 9 + 20;
		archivefilename = malloc(len);
		snprintf(archivefilename, len, "%s/archive/%d", listdir,
			 mindex);
	}

	if((retval = checkwait_smtpreply(sockfd, MLMMJ_CONNECT)) != 0) {
		log_error(LOG_ARGS, "No proper greeting to our connect\n"
			  "We continue and hope for the best\n");
		/* FIXME: Queue etc. */
	}	
	write_helo(sockfd, "localhost");
	if((checkwait_smtpreply(sockfd, MLMMJ_HELO)) != 0) {
		log_error(LOG_ARGS, "Error with HELO\n"
			  "We continue and hope for the best\n");
		/* FIXME: quit and tell admin to configure correctly */
	}

	/* FIXME: use myfgetline instead! */
	switch(listctrl[0]) {
	case '1': /* A single mail is to be sent */
		send_mail(sockfd, bounce_adr, to_addr, replyto, mailfile);
		break;
	case '2': /* Moderators */
		while((bufres = fgets(buf, READ_BUFSIZE, subfile))) {
			chomp(buf);
			send_mail(sockfd, bounce_adr, buf, 0, mailfile);
		}
		break;
	default: /* normal list mail */
		while((bufres = fgets(buf, READ_BUFSIZE, subfile))) {
			chomp(buf);
			bounce_adr = bounce_from_adr(buf, listadr,
					archivefilename);
			send_mail(sockfd, bounce_adr, buf, 0, mailfile);
			free(bounce_adr);
		}
		break;
	}

	write_quit(sockfd);
	if((checkwait_smtpreply(sockfd, MLMMJ_QUIT)) != 0) {
		log_error(LOG_ARGS, "Mailserver would not let us QUIT\n"
			  "We close the socket anyway though\n");
	}

	if(listctrl[0] != '1' && listctrl[0] != '2') {
		/* The mail now goes to the archive */
		rename(mailfilename, archivefilename);

		fclose(subfile);
		free(archivefilename);
		free(subfilename);
	} else if(deletewhensent)
		unlink(mailfilename);

	close(sockfd);
	fclose(mailfile);
	return EXIT_SUCCESS;
}
