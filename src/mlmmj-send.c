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
#include <syslog.h>
#include <stdarg.h>

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

int bouncemail(const char *listdir, const char *mlmmjbounce, const char *from)
{
	char *myfrom = strdup(from);
	char *addr, *num, *c;
	size_t len;
	pid_t pid = 0;

	if((c = strchr(myfrom, '@')) == NULL) {
		free(myfrom);
		return 0; /* Success when mailformed 'from' */
	}
	*c = '\0';
	num = strrchr(myfrom, '-');
	num++;
	c = strchr(myfrom, RECIPDELIM);
	myfrom = strchr(c, '-');
	myfrom++;
	len = num - myfrom - 1;
	addr = malloc(len + 1);
	addr[len] = '\0';
	strncpy(addr, myfrom, len);

	pid = fork();
	
	if(pid < 0) {
		log_error(LOG_ARGS, "fork() failed!");
		return 1;
	}
	
	if(pid > 0)
		return 0;
	
	execlp(mlmmjbounce, mlmmjbounce,
			"-L", listdir,
			"-a", addr,
			"-n", num, 0);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjbounce);

	return 1;
}

int send_mail(int sockfd, const char *from, const char *to,
	      const char *replyto, FILE *mailfile,
	      const char *listdir, const char *mlmmjbounce)
{
	int retval = 0;
	char *reply;

	retval = write_mail_from(sockfd, from);
	if(retval) {
		log_error(LOG_ARGS, "Could not write MAIL FROM\n");
		return retval;
	}
	reply = checkwait_smtpreply(sockfd, MLMMJ_FROM);
	if(reply) {
		log_error(LOG_ARGS, "Error in MAIL FROM. Reply = [%s]",
				reply);
		free(reply);
		write_rset(sockfd);
		checkwait_smtpreply(sockfd, MLMMJ_RSET);
		return MLMMJ_FROM;
	}
	retval = write_rcpt_to(sockfd, to);
	if(retval) {
		log_error(LOG_ARGS, "Could not write RCPT TO:\n");
		return retval;
	}

	reply = checkwait_smtpreply(sockfd, MLMMJ_RCPTTO);
	if(reply) {
		write_rset(sockfd);
		checkwait_smtpreply(sockfd, MLMMJ_RSET);
		if(mlmmjbounce && (reply[1] == '5') && ((reply[0] == '4') ||
					(reply[0] == '5'))) {
			free(reply);
			return bouncemail(listdir, mlmmjbounce, from);
		} else {
			log_error(LOG_ARGS, "Error in RCPT TO. Reply = [%s]",
					reply);
			free(reply);
			return MLMMJ_RCPTTO;
		}
	}

	retval = write_data(sockfd);
	if(retval) {
		log_error(LOG_ARGS, "Could not write DATA\b");
		return retval;
	}

	reply = checkwait_smtpreply(sockfd, MLMMJ_DATA);
	if(reply) {
		log_error(LOG_ARGS, "Error with DATA. Reply = [%s]", reply);
		free(reply);
		write_rset(sockfd);
		checkwait_smtpreply(sockfd, MLMMJ_RSET);
		return MLMMJ_DATA;
	}

	if(replyto) {
		retval = write_replyto(sockfd, replyto);
		if(retval) {
			log_error(LOG_ARGS, "Could not write reply-to addr.\n");
			return retval;
		}
	}

	retval = write_mailbody_from_file(sockfd, mailfile);
	if(retval) {
		log_error(LOG_ARGS, "Could not write mailbody\n");
		return retval;
	}

	retval = write_dot(sockfd);
	if(retval) {
		log_error(LOG_ARGS, "Could not write <CR><LF>.<CR><LF>\n");
		return retval;
	}

	reply = checkwait_smtpreply(sockfd, MLMMJ_DOT);
	if(reply) {
		log_error(LOG_ARGS, "Mailserver did not ack end of mail.\n"
				"<CR><LF>.<CR><LF> was written, to no"
				"avail. Reply = [%s]", reply);
		free(reply);
		write_rset(sockfd);
		checkwait_smtpreply(sockfd, MLMMJ_RSET);
		return MLMMJ_DOT;
	}

	return 0;
}

int initsmtp(int *sockfd, const char *relayhost)
{
	int retval = 0;
	char *reply = NULL;
	char *myhostname = hostnamestr();
	
	init_sockfd(sockfd, relayhost);
	
	if((reply = checkwait_smtpreply(*sockfd, MLMMJ_CONNECT)) != NULL) {
		log_error(LOG_ARGS, "No proper greeting to our connect"
			  "Reply: [%s]", reply);
		free(reply);
		retval = MLMMJ_CONNECT;
		/* FIXME: Queue etc. */
	}	
	write_helo(*sockfd, myhostname);
	free(myhostname);
	if((reply = checkwait_smtpreply(*sockfd, MLMMJ_HELO)) != NULL) {
		log_error(LOG_ARGS, "Error with HELO. Reply: [%s]", reply);
		/* FIXME: quit and tell admin to configure correctly */
		free(reply);
		retval = MLMMJ_HELO;
	}

	return retval;
}

int endsmtp(int *sockfd)
{
	int retval = 0;
	char *reply;
	
	write_quit(*sockfd);

	if((reply = checkwait_smtpreply(*sockfd, MLMMJ_QUIT)) != 0) {
		log_error(LOG_ARGS, "Mailserver would not let us QUIT. "
			  "We close the socket anyway though.");
		free(reply);
		retval = MLMMJ_QUIT;
	}

	close(*sockfd);

	return retval;
}

int send_mail_many(int sockfd, const char *from, const char *replyto,
		   FILE *mailfile, FILE *subfile, const char *listaddr,
		   const char *archivefilename, const char *listdir,
		   const char *mlmmjbounce)
{
	int sendres = 0;
	char *bounceaddr, *addr, *index, *dirname, *addrfilename;
	char *myarchivefilename = strdup(archivefilename);
	FILE *addrfile;

	while((addr = myfgetline(subfile))) {
		chomp(addr);
		if(from)
			sendres = send_mail(sockfd, from, addr, replyto,
					    mailfile, listdir, NULL);
		else {
			bounceaddr = bounce_from_adr(addr, listaddr,
						     archivefilename);
			sendres = send_mail(sockfd, bounceaddr, addr, replyto,
				  mailfile, listdir, mlmmjbounce);
			free(bounceaddr);
		}
		if(sendres && listaddr && archivefilename) {
			/* we failed, so save the addresses and bail */
			index = basename(myarchivefilename);	
			free(myarchivefilename);
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
			addrfilename = concatstr(2, dirname, "/subscribers");
			free(dirname);
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

static void print_help(const char *prg)
{
        printf("Usage: %s [-L /path/to/list || -l listctrl] -m /path/to/mail "
	       "[-D] [-F] [-h]\n"
	       "       [-r] [-R] [-T] [-V]\n"
	       " -D: Don't delete the mail after it's sent\n"
	       " -F: What to use as MAIL FROM:\n"
	       " -h: This help\n"
	       " -l: List control variable:\n"
	       "    '1' means 'send a single mail'\n"
	       "    '2' means 'mail to moderators'\n"
	       " -L: Full path to list directory\n"
	       " -m: Full path to mail file\n"
	       " -r: Relayhost (defaults to localhost)\n"
	       " -R: What to use as Reply-To: header\n"
	       " -T: What to use as RCPT TO:\n"
	       " -V: Print version\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	size_t len = 0;
	int sockfd = 0, opt, mindex;
	int deletewhensent = 1, *newsockfd, sendres;
	char *listaddr, *mailfilename = NULL, *subfilename = NULL;
	char *replyto = NULL, *bounceaddr = NULL, *to_addr = NULL;
	char *relayhost = NULL, *archivefilename = NULL, *tmpstr;
	char *listctrl = NULL, *subddirname = NULL, *listdir = NULL;
	char *mlmmjbounce = NULL, *argv0 = strdup(argv[0]);
	DIR *subddir;
	FILE *subfile = NULL, *mailfile = NULL, *tmpfile;
	struct dirent *dp;
	pid_t childpid;
	struct sigaction sigact;
	
	log_set_name(argv[0]);

	mlmmjbounce = concatstr(2, dirname(argv0), "/mlmmj-bounce");
	free(argv0);
	
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
				replyto, mailfile, listdir, NULL);
		endsmtp(&sockfd);
		if(sendres) {
			/* error, so keep it in the queue */
			deletewhensent = 0;
			/* dump date we want when resending */
			tmpstr = concatstr(2, mailfilename, ".mailfrom");
			tmpfile = fopen(tmpstr, "w");
			free(tmpstr);
			fputs(bounceaddr, tmpfile);
			fclose(tmpfile);
			tmpstr = concatstr(2, mailfilename, ".reciptto");
			tmpfile = fopen(tmpstr, "w");
			free(tmpstr);
			fputs(to_addr, tmpfile);
			fclose(tmpfile);
			if(replyto) {
				tmpstr = concatstr(2, mailfilename,
						      ".reply-to");
				tmpfile = fopen(tmpstr, "w");
				free(tmpstr);
				fputs(replyto, tmpfile);
				fclose(tmpfile);
			}
		}
		break;
	case '2': /* Moderators */
		initsmtp(&sockfd, relayhost);
		send_mail_many(sockfd, bounceaddr, NULL, mailfile, subfile,
			       NULL, NULL, listdir, NULL);
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
					       archivefilename, listdir,
					       mlmmjbounce);
				endsmtp(newsockfd);
				free(newsockfd);
				exit(EXIT_SUCCESS);
			} else {
				syslog(LOG_INFO, "%d/%d connections open",
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
