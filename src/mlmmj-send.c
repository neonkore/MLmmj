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
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <libgen.h>

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
#include "mygetline.h"

static int conncount = 0;  /* Connection count */

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/chat-list -m /path/to/mail\n", prg);
	exit(EXIT_SUCCESS);
}

char *bounce_from_adr(const char *recipient, const char *listadr,
		      const char *mailfilename)
{
	char *bounceaddr, *myrecipient, *mylistadr;
	char *indexstr, *listdomain, *a, *mymailfilename;
	size_t len;

	mymailfilename = strdup(mailfilename);
	if (!mymailfilename) {
		return NULL;
	}

	indexstr = strrchr(mymailfilename, '/');
	if (indexstr) {
		indexstr++;  /* skip the slash */
	} else {
		indexstr = mymailfilename;
	}

	myrecipient = strdup(recipient);
	if (!myrecipient) {
		free(mymailfilename);
		return NULL;
	}
	a = strchr(myrecipient, '@');
	if (a) *a = '=';

	mylistadr = strdup(listadr);
	if (!mylistadr) {
		free(mymailfilename);
		free(myrecipient);
		return NULL;
	}

	listdomain = strchr(mylistadr, '@');
	if (!listdomain) {
		free(mymailfilename);
		free(myrecipient);
		free(mylistadr);
		return NULL;
	}
	*listdomain++ = '\0';

	/* 12 = RECIPDELIM + "bounces-" + "-" + "@" + NUL */
	len = strlen(mylistadr) + strlen(myrecipient) + strlen(indexstr)
		 + strlen(listdomain) + 12;
	bounceaddr = malloc(len);
	if (!bounceaddr) {
		free(myrecipient);
		free(mylistadr);
		return NULL;
	}
	snprintf(bounceaddr, len, "%s%cbounces-%s-%s@%s", mylistadr, RECIPDELIM,
		 myrecipient, indexstr, listdomain);

	free(myrecipient);
	free(mylistadr);
	free(mymailfilename);

	return bounceaddr;
}

int send_mail(int sockfd, const char *from, const char *to,
	      const char *replyto, FILE *mailfile)
{
	int retval = 0;

	if((retval = write_mail_from(sockfd, from)) != 0) {
		log_error(LOG_ARGS, "Could not write MAIL FROM\n");
		return retval;
	}
	if((retval = checkwait_smtpreply(sockfd, MLMMJ_FROM)) != 0) {
		log_error(LOG_ARGS, "Wrong MAIL FROM:\n");
		return retval;
	}

	if((retval = write_rcpt_to(sockfd, to)) != 0) {
		log_error(LOG_ARGS, "Could not write RCPT TO:\n");
		return retval;
	}
	if((retval = checkwait_smtpreply(sockfd, MLMMJ_RCPTTO)) != 0) {
		log_error(LOG_ARGS, "Wrong RCPT TO:\n");
		return retval;
	}
	if((retval = write_data(sockfd)) != 0) {
		log_error(LOG_ARGS, "Could not write DATA\b");
		return retval;
	}
	if((retval = checkwait_smtpreply(sockfd, MLMMJ_DATA)) != 0) {
		log_error(LOG_ARGS, "Mailserver not ready for DATA\n");
		return retval;
	}
	if(replyto)
		if((retval = write_replyto(sockfd, replyto)) != 0) {
			log_error(LOG_ARGS, "Could not write reply-to addr.\n");
			return retval;
		}
	if((retval = write_mailbody_from_file(sockfd, mailfile)) != 0) {
		log_error(LOG_ARGS, "Could not write mailbody\n");
		return retval;
	}

	if((retval = write_dot(sockfd)) != 0) {
		log_error(LOG_ARGS, "Could not write <CR><LF>.<CR><LF>\n");
		return retval;
	}

	if((retval = checkwait_smtpreply(sockfd, MLMMJ_DOT)) != 0) {
		log_error(LOG_ARGS, "Mailserver did not ack end of mail.\n"
				"<CR><LF>.<CR><LF> was written, to no"
				"avail\n");
		return retval;
	}

	return 0;
}

int initsmtp(int *sockfd, const char *relayhost)
{
	int retval = 0;
	
	init_sockfd(sockfd, relayhost);
	
	if((retval = checkwait_smtpreply(*sockfd, MLMMJ_CONNECT)) != 0) {
		log_error(LOG_ARGS, "No proper greeting to our connect\n"
			  "We continue and hope for the best\n");
		/* FIXME: Queue etc. */
	}	
	write_helo(*sockfd, relayhost);
	if((checkwait_smtpreply(*sockfd, MLMMJ_HELO)) != 0) {
		log_error(LOG_ARGS, "Error with HELO\n"
			  "We continue and hope for the best\n");
		/* FIXME: quit and tell admin to configure correctly */
	}

	return retval;
}

int endsmtp(int *sockfd)
{
	int retval = 0;
	
	write_quit(*sockfd);

	if((retval = checkwait_smtpreply(*sockfd, MLMMJ_QUIT)) != 0) {
		log_error(LOG_ARGS, "Mailserver would not let us QUIT\n"
			  "We close the socket anyway though\n");
	}

	close(*sockfd);

	return retval;
}

int send_mail_many(int sockfd, const char *from, const char *replyto,
		   FILE *mailfile, FILE *subfile, const char *listaddr,
		   const char *archivefilename, const char *listdir)
{
	int sendres = 0;
	char *bounceaddr, *addr, *index, *dirname, *addrfilename;
	FILE *addrfile;

	while((addr = myfgetline(subfile))) {
		chomp(addr);
		if(from)
			sendres = send_mail(sockfd, from, addr, replyto,
					    mailfile);
		else {
			bounceaddr = bounce_from_adr(addr, listaddr,
						     archivefilename);
			sendres = send_mail(sockfd, bounceaddr, addr, replyto,
				  mailfile);
			free(bounceaddr);
		}
		if(sendres && listaddr && archivefilename) {
			/* we failed, so save the addresses and bail */
			index = basename(archivefilename);	
			dirname = concatstr(3, listdir, "/requeue/", index);
			free(index);
			if(mkdir(dirname, 0750) < 0) {
				log_error(LOG_ARGS, "Could not mkdir(%s) for "
						    "requeueing. Mail cannot "
						    "be requeued.", dirname);
				free(dirname);
				free(addr);
				return 1;
			}
			free(dirname);
			addrfilename = concatstr(2, dirname, "/subscribers");
			addrfile = fopen(addrfilename, "w");
			if(addrfile == NULL) {
				log_error(LOG_ARGS, "Could not create %s",
						    addrfilename);
				free(addrfilename);
				free(addr);
				return 1;
			} else { /* dump the remaining addresses */
				do {
					if(fputs(addr, addrfile) < 0)
						log_error(LOG_ARGS,
							"Could not add [%s] "
							"to requeue address "
							"file.", addr);
					fputc('\n', addrfile);
					free(addr);
					addr = myfgetline(subfile);
				} while(addr);
			}
			
			free(addr);
			free(addrfilename);
			fclose(addrfile);

			return 1;
		}
	}
	return 0;
}	

void sig_child(int sig)
{
	pid_t pid;
	int stat;

	while((pid = waitpid(-1, &stat, WNOHANG) > 0))
		conncount--;
}

int main(int argc, char **argv)
{
	size_t len = 0;
	int sockfd = 0, opt, mindex;
	FILE *subfile = NULL, *mailfile = NULL;
	char *listaddr, *mailfilename = NULL, *subfilename = NULL;
	char *replyto = NULL, *bounceaddr = NULL, *to_addr = NULL;
	char *relayhost = NULL, *archivefilename = NULL;
	char *listctrl = NULL, *subddirname = NULL, *listdir = NULL;
	int deletewhensent = 1, *newsockfd, sendres;
	DIR *subddir;
	struct dirent *dp;
	pid_t childpid;
	struct sigaction sigact;
	
	log_set_name(argv[0]);

	while ((opt = getopt(argc, argv, "VDhm:l:L:R:F:T:r:")) != -1){
		switch(opt) {
		case 'D':
			deletewhensent = 0;
			break;
		case 'F':
			bounceaddr = optarg;
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
	if(listctrl[0] == '1' && (bounceaddr == NULL || to_addr == NULL)) {
		fprintf(stderr, "With -l 1 you need -F and -T\n");
		exit(EXIT_FAILURE);
	}

	if((listctrl[0] == '2' && (listdir == NULL || bounceaddr == NULL))) {
		fprintf(stderr, "With -l 2 you need -L and -F\n");
		exit(EXIT_FAILURE);
	}

	if(listctrl[0] != '1' && listctrl[0] != '2')
		listaddr = getlistaddr(listdir);
	
	/* initialize file with mail to send */

	if((mailfile = fopen(mailfilename, "r")) == NULL) {
	        log_error(LOG_ARGS, "Could not open '%s'", mailfilename);
		exit(EXIT_FAILURE);
	}

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
	default: /* normal list mail -- now handled when forking */
		break;
	}

	/* initialize the archive filename */
	if(listctrl[0] != '1' && listctrl[0] != '2') {
		mindex = incindexfile((const char *)listdir);
		len = strlen(listdir) + 9 + 20;
		archivefilename = malloc(len);
		snprintf(archivefilename, len, "%s/archive/%d", listdir,
			 mindex);
	}

	if(!relayhost)
		relayhost = strdup(RELAYHOST);

	switch(listctrl[0]) {
	case '1': /* A single mail is to be sent */
		initsmtp(&sockfd, relayhost);
		sendres = send_mail(sockfd, bounceaddr, to_addr,
				replyto, mailfile);
		if(sendres) /* error, so keep it in the queue */
			deletewhensent = 0;
		endsmtp(&sockfd);
		break;
	case '2': /* Moderators */
		initsmtp(&sockfd, relayhost);
		send_mail_many(sockfd, bounceaddr, NULL, mailfile, subfile,
			       NULL, NULL, listdir);
		endsmtp(&sockfd);
		break;
	default: /* normal list mail */
		subddirname = concatstr(2, listdir, "/subscribers.d/");
		if((subddir = opendir(subddirname)) == NULL) {
			log_error(LOG_ARGS, "Could not opendir(%s)",
					    subddirname);
			free(subddirname);
		}
		free(subddirname);

		sigact.sa_handler = sig_child;
		sigemptyset(&sigact.sa_mask);
		sigact.sa_flags = SA_NOCLDSTOP;
		sigaction(SIGCHLD, &sigact, 0);

		while((dp = readdir(subddir)) != NULL) {
			if(!strcmp(dp->d_name, "."))
				continue;
			if(!strcmp(dp->d_name, ".."))
				continue;
			subfilename = concatstr(3, listdir, "/subscribers.d/",
						dp->d_name);
			if((subfile = fopen(subfilename, "r")) == NULL) {
				log_error(LOG_ARGS, "Could not open '%s'",
						    subfilename);
				free(subfilename);
				continue;
			}
			fprintf(stderr, "found subfile '%s'\n", subfilename);
			free(subfilename);

			while((conncount >= MAX_CONNECTIONS))
				usleep(WAITSLEEP);

			childpid = fork();
			if(childpid < 0)
				log_error(LOG_ARGS, "Could not fork.");
				/* TODO: we have to keep track of unsent
				 * files */

			conncount++;

			if(childpid == 0) {
				newsockfd = malloc(sizeof(int));
				initsmtp(newsockfd, relayhost);
				send_mail_many(*newsockfd, NULL, NULL,
					       mailfile, subfile, listaddr,
					       archivefilename, listdir);
				endsmtp(newsockfd);
				free(newsockfd);
				exit(EXIT_SUCCESS);
			} else {
				log_error(LOG_ARGS, "%d/%d connections open",
						conncount, MAX_CONNECTIONS);
			}
		}
		closedir(subddir);
		break;
	}
	
	if(listctrl[0] != '1' && listctrl[0] != '2') {
		/* It is safe to rename() the mail file at this point, because
		   the child processes (who might still be running) inherit a
		   handle to the open file, so they don't care if it is moved
		   or deleted. */

		rename(mailfilename, archivefilename);

		fclose(subfile);
		free(archivefilename);
	} else if(deletewhensent)
		unlink(mailfilename);

	fclose(mailfile);
	return EXIT_SUCCESS;
}
