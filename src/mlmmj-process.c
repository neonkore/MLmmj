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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#include "mlmmj.h"
#include "wrappers.h"
#include "find_email_adr.h"
#include "incindexfile.h"
#include "getlistaddr.h"
#include "listcontrol.h"
#include "strgen.h"
#include "do_all_the_voodo_here.h"
#include "log_error.h"
#include "mygetline.h"
#include "statctrl.h"
#include "ctrlvalue.h"

void newmoderated(const char *listdir, const char *mailfilename,
		  const char *mlmmjsend)
{
	char *to, *from, *subject, *fqdn, *listname, *replyto;
	char *buf, *moderatorfilename, *listaddr = getlistaddr(listdir);
	char *queuefilename, *moderatorsfilename, *randomstr = random_str();
	char *mailbasename = mybasename(mailfilename);
	FILE *moderatorfile, *queuefile, *moderatorsfile, *mailfile;
	size_t count = 0;
	
	printf("mailfilename = [%s], mailbasename = [%s]\n", mailfilename,
			                                     mailbasename);

	fqdn = genlistfqdn(listaddr);
	listname = genlistname(listaddr);
	moderatorfilename = concatstr(2, listdir, "/text/moderation");
	if((moderatorfile = fopen(moderatorfilename, "r")) == NULL) {
		log_error(LOG_ARGS, "Could not open text/moderation");
		free(moderatorfilename);
		exit(EXIT_FAILURE);
	}
	queuefilename = concatstr(3, listdir, "/moderation/queue", randomstr);
	printf("%s\n", queuefilename);
	
	if((queuefile = fopen(queuefilename, "w")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		free(queuefilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	free(randomstr);

	moderatorsfilename = concatstr(2, listdir, "/moderators");
	if((moderatorsfile = fopen(moderatorsfilename, "r")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", moderatorsfilename);
		free(queuefilename);
		free(moderatorsfilename);
		fclose(queuefile);
		exit(EXIT_FAILURE);
	}
	free(moderatorfilename);

	if((mailfile = fopen(mailfilename, "r")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", mailfilename);
		free(queuefilename);
		free(moderatorsfilename);
		fclose(queuefile);
		exit(EXIT_FAILURE);
	}

	fputs("From: ", queuefile);
	from = concatstr(3, listname, "+owner@", fqdn);
	fputs(from, queuefile);
	fputc('\n', queuefile);
	to = concatstr(5, "To: ", listname, "-moderators@", fqdn, "\n");
	fputs(to, queuefile);
	free(to);
	replyto = concatstr(7, "Reply-To: ", listname, "+moderate-",
			       mailbasename, "@", fqdn, "\n");
	fputs(replyto, queuefile);
	free(replyto);
	subject = concatstr(3, "Subject: Moderation needed for ", listaddr,
			       "\n\n");
	fputs(subject, queuefile);
	free(subject);
	
	while((buf = myfgetline(moderatorfile))) {
		if(strncmp(buf, "*LISTADDR*", 10) == 0) {
			fputs(listaddr, queuefile);
		} else if(strncmp(buf, "*MODERATEADDR*", 14) == 0) {
			fputs(listname, queuefile);
			fputs("+moderate-", queuefile);
			fputs(mailbasename, queuefile);
			fputc('@', queuefile);
			fputs(fqdn, queuefile);
		} else if(strncmp(buf, "*MODERATORS*", 12) == 0) {
			free(buf);
			while((buf = myfgetline(moderatorsfile))) {
				fputs(buf, queuefile);
				free(buf);
				buf = NULL;
			}
		} else
			fputs(buf, queuefile);
		free(buf);
	}
	fclose(moderatorfile);
	while((buf = myfgetline(mailfile)) && count < 100) {
		fputc(' ', queuefile);
		fputs(buf, queuefile);
		free(buf);
		count++;
	}
	fclose(queuefile);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "2",
				"-L", listdir,
				"-F", from,
				"-m", queuefilename, 0);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);

	exit(EXIT_FAILURE);
}

static void print_help(const char *prg)
{
        printf("Usage: %s -L /path/to/list -m /path/to/mail [-h] [-P] [-V]\n"
	       " -h: This help\n"
	       " -L: Full path to list directory\n"
	       " -m: Full path to mail file\n"
	       " -P: Don't execute mlmmj-send\n"
	       " -V: Print version\n", prg);

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int i, fd, opt, noprocess = 0, moderated = 0;
	char *listdir = NULL, *mailfile = NULL, *headerfilename = NULL;
	char *footerfilename = NULL, *donemailname = NULL;
	char *randomstr = random_str(), *mqueuename;
	char *mlmmjsend, *mlmmjsub, *mlmmjunsub, *mlmmjbounce;
	char *bindir, *subjectprefix, *discardname;
	FILE *headerfile, *footerfile, *rawmailfile, *donemailfile;
	struct email_container fromemails = { 0, NULL };
	struct email_container toemails = { 0, NULL };
	struct email_container ccemails = { 0, NULL };
	const char *badheaders[] = { "From ", "Return-Path:", NULL };
	struct mailhdr readhdrs[] = {
		{ "From:", 0, NULL },
		{ "To:", 0, NULL },
		{ "Cc:", 0, NULL },
		{ NULL, 0, NULL }
	};

	log_set_name(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjsend = concatstr(2, bindir, "/mlmmj-send");
	mlmmjsub = concatstr(2, bindir, "/mlmmj-sub");
	mlmmjunsub = concatstr(2, bindir, "/mlmmj-unsub");
	mlmmjbounce = concatstr(2, bindir, "/mlmmj-bounce");
	free(bindir);
	
	while ((opt = getopt(argc, argv, "hVPm:L:")) != -1) {
		switch(opt) {
		case 'L':
			listdir = optarg;
			break;
		case 'm':
			mailfile = optarg;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'P':
			noprocess = 1;
			break;
		case 'V':
			print_version(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}
	if(listdir == NULL || mailfile == NULL) {
		fprintf(stderr, "You have to specify -L and -m\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	donemailname = concatstr(3, listdir, "/queue/", randomstr);
	fd = open(donemailname, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	while(fd == -1 && errno == EEXIST) {
		free(donemailname);
		randomstr = random_str();
		donemailname = concatstr(3, listdir, "/queue/", randomstr);
		fd = open(donemailname, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	}
	
	if(fd == -1) {
		free(donemailname);
		log_error(LOG_ARGS, "could not create mail file in queue"
				    "directory");
		exit(EXIT_FAILURE);
	}

	if((donemailfile = fdopen(fd, "w")) == NULL) {
		free(donemailname);
		log_error(LOG_ARGS, "could not fdopen() output mail file");
		exit(EXIT_FAILURE);
	}

#if 0
	log_error(LOG_ARGS, "[%s]\n", donemailname);
#endif
	if((rawmailfile = fopen(mailfile, "r")) == NULL) {
		free(donemailname);
		log_error(LOG_ARGS, "could not fopen() input mail file");
		exit(EXIT_FAILURE);
	}

	headerfilename = concatstr(2, listdir, "/control/customheaders");
	headerfile = fopen(headerfilename, "r");
	free(headerfilename);
	
	footerfilename = concatstr(2, listdir, "/control/footer");
	footerfile = fopen(footerfilename, "r");
	free(footerfilename);
	
	subjectprefix = ctrlvalue(listdir, "prefix");	
	
	do_all_the_voodo_here(rawmailfile, donemailfile, headerfile,
				footerfile, badheaders, readhdrs,
				subjectprefix);

	fclose(rawmailfile);
	unlink(mailfile);
	close(fd);
	fclose(donemailfile);

	if(headerfile)
		fclose(headerfile);
	if(footerfile)
		fclose(footerfile);

	if(readhdrs[0].token) { /* From: addresses */
		for(i = 0; i < readhdrs[0].valuecount; i++) {
			find_email_adr(readhdrs[0].values[i], &fromemails);
		}
		if(fromemails.emailcount != 1) { /* discard malformed mail */
			discardname = concatstr(3, listdir,
						"/queue/discarded/",
						randomstr);
			rename(donemailname, discardname);
			free(donemailname);
			free(discardname);
			/* TODO: free emailstructs */
			exit(EXIT_SUCCESS);
		}
	}

	if(readhdrs[1].token) { /* To: addresses */
		for(i = 0; i < readhdrs[1].valuecount; i++) {
			find_email_adr(readhdrs[1].values[i], &toemails);
		}
	}

	if(readhdrs[2].token) { /* Cc: addresses */
		for(i = 0; i < readhdrs[1].valuecount; i++) {
			find_email_adr(readhdrs[1].values[i], &ccemails);
		}
	}
	if(strchr(toemails.emaillist[0], RECIPDELIM)) {
#if 0
		log_error(LOG_ARGS, "listcontrol(from, %s, %s, %s, %s, %s, %s)\n", listdir, toemails.emaillist[0], mlmmjsub, mlmmjunsub, mlmmjsend, mlmmjbounce);
#endif
		listcontrol(&fromemails, listdir, toemails.emaillist[0],
			    mlmmjsub, mlmmjunsub, mlmmjsend, mlmmjbounce);
		return EXIT_SUCCESS;
	}
#if 0
	for(i = 0; i < toemails.emailcount; i++) {
		if(strncmp(listaddr, toemails.emaillist[i],
					strlen(toemails.emaillist[i]))
#endif

	moderated = statctrl(listdir, "moderated");

	if(moderated) {
		mqueuename = concatstr(3, listdir, "/moderation/",
				       randomstr);
		printf("Going into moderatemode, mqueuename = [%s]\n",
				mqueuename);
		free(randomstr);
		if(rename(donemailname, mqueuename) < 0) {
			printf("could not rename(%s,%s)\n", 
					    donemailname, mqueuename);
			perror("rename");
			log_error(LOG_ARGS, "could not rename(%s,%s)", 
					    donemailname, mqueuename);
			free(donemailname);
			free(mqueuename);
			exit(EXIT_FAILURE);
		}
		free(donemailname);
		newmoderated(listdir, mqueuename, mlmmjsend);
		return EXIT_SUCCESS;
	}


	if(noprocess) {
		free(donemailname);
		/* XXX: toemails and ccemails etc. have to be free() */
		exit(EXIT_SUCCESS);
	}
	
	execlp(mlmmjsend, mlmmjsend,
				"-L", listdir,
				"-m", donemailname, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);

	return EXIT_FAILURE;
}
