/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
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

#include "log_error.c"


static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/chat-list -m /path/to/mail\n", prg);
	exit(EXIT_SUCCESS);
}

char *bounce_from_adr(char *bounce_adr, const char *from, const
		char *listadr, const char *mailfilename)
{
	char *tmp;
	char *i;
	char indexstr[20];
	int j, k;

	for(j = strlen(mailfilename)-1, k = 0; mailfilename[j] != '/'; j--, k++)
		indexstr[k] = mailfilename[j];
	indexstr[k] = 0;
	
	reversestr(indexstr);

	tmp = malloc(strlen(from) + 1);
	memcpy(tmp, from, strlen(from) + 1);
	i = index(tmp, '@');
	*i = '=';
	i = index(listadr, '@');

	memcpy(bounce_adr, listadr, i - listadr);
	strncat(bounce_adr, "-bounces", 8);
	bounce_adr[strlen(bounce_adr)] = RECIPDELIM;
	strncat(bounce_adr, tmp, strlen(tmp));
	bounce_adr[strlen(bounce_adr)] = '-';
	bounce_adr[strlen(bounce_adr)+1] = 0;
	strncat(bounce_adr, indexstr, strlen(indexstr));
	strncat(bounce_adr, i, strlen(i));
	free(tmp);

	return bounce_adr;
}

int send_mail(int sockfd, const char *from, const char *to, const char *replyto,
		FILE *mailfile)
{
	int retval;

	if((retval = write_mail_from(sockfd, from)) != 0) {
		log_error("Could not write MAIL FROM\n");
		/* FIXME: Queue etc.*/
		write_rset(sockfd);
		return retval;
	}
	if((retval = checkwait_smtpreply(sockfd, MLMMJ_FROM)) != 0) {
		log_error("Wrong MAIL FROM:\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}

	if((retval = write_rcpt_to(sockfd, to)) != 0) {
		log_error("Could not write RCPT TO:\n");
		/* FIXME: Queue etc.*/
		write_rset(sockfd);
		return retval;
	}
	if((retval = checkwait_smtpreply(sockfd, MLMMJ_RCPTTO)) != 0) {
		log_error("Wrong RCPT TO:\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}
	if((retval = write_data(sockfd)) != 0) {
		log_error("Could not write DATA\b");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}
	if((checkwait_smtpreply(sockfd, MLMMJ_DATA)) != 0) {
		log_error("Mailserver not ready for DATA\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}
	if(replyto)
		if((retval = write_replyto(sockfd, replyto)) != 0) {
			log_error("Could not write reply-to addr.\n");
			write_rset(sockfd);
			/* FIXME: Queue etc.*/
			return retval;
		}
	if((retval = write_mailbody_from_file(sockfd, mailfile)) != 0) {
		log_error("Could not write mailbody\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}

	if((retval = write_dot(sockfd)) != 0) {
		log_error("Could not write <CR><LF>.<CR><LF>\n");
		write_rset(sockfd);
		/* FIXME: Queue etc.*/
		return retval;
	}

	if((checkwait_smtpreply(sockfd, MLMMJ_DOT)) != 0) {
		log_error("Mailserver did not acknowledge end of mail\n"
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
	int sockfd = 0, opt, mindex, retval=0;
	FILE *subfile = 0, *mailfile = 0;
	char *mailfilename = 0;
	char *subfilename = 0;
	char *listdir = 0;
	char listadr[READ_BUFSIZE];
	char *replyto = 0;
	char *bounce_adr = 0;
	char *to_addr = 0;
	char *archivefilename = 0;
	char buf[READ_BUFSIZE];
	char *bufres;
	char *relayhost = 0;
	int deletewhensent = 1;
	
	log_set_name(argv[0]);

	while ((opt = getopt(argc, argv, "VDhm:L:R:F:T:r:")) != -1){
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
			exit(0);
		}
	}

	if(mailfilename == 0 || listdir == 0) {
		fprintf(stderr, "You have to specify -L and -m\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	/* get the list address */
	if(listdir[0] == '1' && (bounce_adr == 0 || to_addr == 0))
		fprintf(stderr, "With -L 1 you need -F and -T\n");

	if(listdir[0] != '1')
		getlistaddr(listadr, listdir);
	
	/* initialize file with mail to send */

	if((mailfile = fopen(mailfilename, "r")) == NULL) {
	        log_error(mailfilename);
		exit(EXIT_FAILURE);
	}

	if(relayhost)
		init_sockfd(&sockfd, relayhost);
	else
		init_sockfd(&sockfd, RELAYHOST);
	
	/* XXX: Here is the subscribers unrolled and mails are sent.
	 * A more intelligent generic solution will be implemented
	 * later, so we can get LDAP, SQL etc. support for the 
	 * subscribers list. Choeger?
	 */
	if(listdir[0] != '1') {
		subfilename = genfilename(listdir, "/subscribers");
		if((subfile = fopen(subfilename, "r")) == NULL) {
			log_error("Could not open subscriberfile:");
			exit(EXIT_FAILURE);
		}
	}

	/* initialize the archive filename */
	if(listdir[0] != '1') {
		mindex = incindexfile((const char *)listdir, 1);
		len = strlen(listdir) + 9 + 20;
		archivefilename = malloc(len);
		snprintf(archivefilename, len, "%s/archive/%d", listdir, mindex);
	}

	if((retval = checkwait_smtpreply(sockfd, MLMMJ_CONNECT)) != 0) {
		log_error("No proper greeting to our connect\n"
			  "We continue and hope for the best\n");
		/* FIXME: Queue etc. */
	}	
	write_helo(sockfd, "localhost");
	if((checkwait_smtpreply(sockfd, MLMMJ_HELO)) != 0) {
		log_error("Error with HELO\n"
			  "We continue and hope for the best\n");
		/* FIXME: quit and tell admin to configure correctly */
	}

	if(listdir[0] != '1') {
		while((bufres = fgets(buf, READ_BUFSIZE, subfile))) {
			chomp(buf);
			len = strlen(buf) + strlen(listadr) + 256;
			bounce_adr = malloc(len);
			memset(bounce_adr, 0, len);
			bounce_from_adr(bounce_adr, buf, listadr,
					archivefilename);

			send_mail(sockfd, bounce_adr, buf, 0, mailfile);
			free(bounce_adr);
		}
	} else
		send_mail(sockfd, bounce_adr, to_addr, replyto, mailfile);

	write_quit(sockfd);
	if((checkwait_smtpreply(sockfd, MLMMJ_QUIT)) != 0) {
		log_error("Mailserver would not let us QUIT\n"
			  "We close the socket anyway though\n");
	}

	if(listdir[0] != '1') {
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
