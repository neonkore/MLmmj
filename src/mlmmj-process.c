/* Copyright (C) 2002, 2003, 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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
#include "ctrlvalues.h"
#include "getlistaddr.h"
#include "prepstdreply.h"
#include "subscriberfuncs.h"

void newmoderated(const char *listdir, const char *mailfilename,
		  const char *mlmmjsend)
{
	char *from, *fqdn, *listname;
	char *buf, *moderatorfilename, *listaddr = getlistaddr(listdir);
	char *queuefilename, *moderatorsfilename, *randomstr = random_str();
	char *mailbasename = mybasename(mailfilename), *s1;
	int moderatorfd, queuefd, moderatorsfd, mailfd;
	size_t count = 0;
#if 0
	printf("mailfilename = [%s], mailbasename = [%s]\n", mailfilename,
			                                     mailbasename);
#endif
	fqdn = genlistfqdn(listaddr);
	listname = genlistname(listaddr);
	moderatorfilename = concatstr(2, listdir, "/text/moderation");
	if((moderatorfd = open(moderatorfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open text/moderation");
		free(moderatorfilename);
		exit(EXIT_FAILURE);
	}
	free(moderatorfilename);

	moderatorsfilename = concatstr(2, listdir, "/control/moderators");
	if((moderatorsfd = open(moderatorsfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", moderatorsfilename);
		free(queuefilename);
		free(moderatorsfilename);
		close(queuefd);
		exit(EXIT_FAILURE);
	}
	free(moderatorsfilename);

	if((mailfd = open(mailfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", mailfilename);
		free(queuefilename);
		free(moderatorsfilename);
		close(queuefd);
		exit(EXIT_FAILURE);
	}

	queuefilename = concatstr(3, listdir, "/queue/", randomstr);
	
	if((queuefd = open(queuefilename, O_WRONLY|O_CREAT|O_EXCL,
					S_IRUSR|S_IWUSR)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		free(queuefilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	free(randomstr);

	from = concatstr(3, listname, "+owner@", fqdn);
	s1 = concatstr(15, "From: ", from, "\nTo: ", listname, "-moderators@",
			fqdn, "\nReply-To: ", listname,	"+moderate-",
			mailbasename, "@", fqdn,
			"\nSubject: Moderation needed for ", listaddr, "\n\n");
	if(writen(queuefd, s1, strlen(s1)) < 0) {
		log_error(LOG_ARGS, "Could not write to %s", queuefilename);
		exit(EXIT_FAILURE);
	}
	free(s1);
	s1 = concatstr(5, listname, "+moderate-", mailbasename, "@", fqdn);
	
	while((buf = mygetline(moderatorfd))) {
		if(strncmp(buf, "*LISTADDR*", 10) == 0) {
			if(writen(queuefd, listaddr, strlen(listaddr)) < 0) {
				log_error(LOG_ARGS, "Could not write to %s",
						queuefilename);
				exit(EXIT_FAILURE);
			}
		} else if(strncmp(buf, "*MODERATEADDR*", 14) == 0) {
			if(writen(queuefd, s1, strlen(s1)) < 0) {
				log_error(LOG_ARGS, "Could not write to %s",
						queuefilename);
				exit(EXIT_FAILURE);
			}
			free(s1);
		} else if(strncmp(buf, "*MODERATORS*", 12) == 0) {
			free(buf);
			while((buf = mygetline(moderatorsfd))) {
				if(writen(queuefd, buf, strlen(buf)) < 0)
					log_error(LOG_ARGS,
						"Could not write moderators");
					
				free(buf);
				buf = NULL;
			}
		} else
			if(writen(queuefd, buf, strlen(buf)) < 0) {
				log_error(LOG_ARGS,
					"Could not write moderatemail");
				exit(EXIT_FAILURE);
			}
		free(buf);
	}
	close(moderatorfd);
	close(moderatorsfd);
	while((buf = mygetline(mailfd)) && count < 100) {
		s1 = concatstr(2, " ", buf);
		free(buf);
		writen(queuefd, s1, strlen(s1));
		free(s1);
		count++;
	}
	close(queuefd);
	close(mailfd);

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
	int hdrfd, footfd, rawmailfd, donemailfd;
	int subonlypost = 0, addrtocc = 1, intocc = 0;
	char *listdir = NULL, *mailfile = NULL, *headerfilename = NULL;
	char *footerfilename = NULL, *donemailname = NULL;
	char *randomstr = random_str(), *mqueuename;
	char *mlmmjsend, *mlmmjsub, *mlmmjunsub, *mlmmjbounce;
	char *bindir, *subjectprefix, *discardname, *listaddr;
	char *listfqdn, *listname, *fromaddr, *fromstr, *subject;
	char *queuefilename, *recipdelim, *owner = NULL;
	char *maildata[4];
	struct email_container fromemails = { 0, NULL };
	struct email_container toemails = { 0, NULL };
	struct email_container ccemails = { 0, NULL };
	struct email_container efromemails = { 0, NULL };
	struct strlist *delheaders;
	struct mailhdr readhdrs[] = {
		{ "From:", 0, NULL },
		{ "To:", 0, NULL },
		{ "Cc:", 0, NULL },
		{ "From ", 0, NULL },
		{ NULL, 0, NULL }
	};

	CHECKFULLPATH(argv[0]);

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
	donemailfd = open(donemailname, O_RDWR|O_CREAT|O_EXCL,
					S_IRUSR|S_IWUSR);
	while(donemailfd < 0 && errno == EEXIST) {
		free(donemailname);
		randomstr = random_str();
		donemailname = concatstr(3, listdir, "/queue/", randomstr);
		fd = open(donemailname, O_RDWR|O_CREAT|O_EXCL,
					S_IRUSR|S_IWUSR);
	}
	
	if(donemailfd < 0) {
		free(donemailname);
		log_error(LOG_ARGS, "could not create mail file in queue"
				    "directory");
		exit(EXIT_FAILURE);
	}
#if 0
	log_error(LOG_ARGS, "donemailname = [%s]\n", donemailname);
#endif
	if((rawmailfd = open(mailfile, O_RDONLY)) < 0) {
		free(donemailname);
		log_error(LOG_ARGS, "could not open() input mail file");
		exit(EXIT_FAILURE);
	}

	headerfilename = concatstr(2, listdir, "/control/customheaders");
	hdrfd = open(headerfilename, O_RDONLY);
	free(headerfilename);
	
	footerfilename = concatstr(2, listdir, "/control/footer");
	footfd = open(footerfilename, O_RDONLY);
	free(footerfilename);

	delheaders = ctrlvalues(listdir, "delheaders");
	if(delheaders == NULL) {
		delheaders = malloc(sizeof(struct strlist));
		delheaders->count = 0;
		delheaders->strs = NULL;
	}

	delheaders->strs = realloc(delheaders->strs,
			(delheaders->count+3) * sizeof(char *));
	delheaders->strs[delheaders->count++] = strdup("From ");
	delheaders->strs[delheaders->count++] = strdup("Return-Path:");
	delheaders->strs[delheaders->count++] = NULL;
	
	subjectprefix = ctrlvalue(listdir, "prefix");	
	
	if(do_all_the_voodo_here(rawmailfd, donemailfd, hdrfd, footfd,
				(const char**)delheaders->strs, readhdrs,
				subjectprefix) < 0) {
		log_error(LOG_ARGS, "Error in do_all_the_voodo_here");
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < delheaders->count; i++)
		free(delheaders->strs[i]);
	free(delheaders->strs);
	free(delheaders);

	close(rawmailfd);
	close(donemailfd);

	if(hdrfd)
		close(hdrfd);
	if(footfd)
		close(footfd);

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
		for(i = 0; i < readhdrs[2].valuecount; i++) {
			find_email_adr(readhdrs[2].values[i], &ccemails);
		}
	}

	if(readhdrs[3].token) { /* From  addresses */
		for(i = 0; i < readhdrs[3].valuecount; i++) {
			find_email_adr(readhdrs[3].values[i], &efromemails);
		}
	}

	recipdelim = strchr(toemails.emaillist[0], RECIPDELIM);

	if(recipdelim) {
		owner = concatstr(2, listdir, "control/owner");
		if(owner && strncmp(recipdelim, "+owner@", 7) == 0) {
			unlink(donemailname);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "4",
					"-F", efromemails.emaillist[0],
					"-s", owner,
					"-a",
					"-m", mailfile, 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsend);
			exit(EXIT_FAILURE);
		}
#if 0
		log_error(LOG_ARGS, "listcontrol(from, %s, %s, %s, %s, %s, %s)\n", listdir, toemails.emaillist[0], mlmmjsub, mlmmjunsub, mlmmjsend, mlmmjbounce);
#endif
		unlink(mailfile);
		listcontrol(&fromemails, listdir, toemails.emaillist[0],
			    mlmmjsub, mlmmjunsub, mlmmjsend, mlmmjbounce,
			    donemailname);
		return EXIT_SUCCESS;
	}

	unlink(mailfile);

	listaddr = getlistaddr(listdir);

	addrtocc = !(statctrl(listdir, "tocc"));
	if(addrtocc) {
		for(i = 0; i < toemails.emailcount; i++)
			if(strcmp(listaddr, toemails.emaillist[i]) == 0)
				intocc = 1;
		for(i = 0; i < ccemails.emailcount; i++)
			if(strcmp(listaddr, ccemails.emaillist[i]) == 0)
				intocc = 1;
	}

	if(addrtocc && !intocc) {
		listname = genlistname(listaddr);
		listfqdn = genlistfqdn(listaddr);
		maildata[0] = "*LSTADDR*";
		maildata[1] = listaddr;
		fromaddr = concatstr(3, listname, "+bounces-help@", listfqdn);
		fromstr = concatstr(3, listname, "+owner@", listfqdn);
		subject = concatstr(3, "Post to ", listaddr, " denied.");
		queuefilename = prepstdreply(listdir, "notintocc", fromstr,
					     fromemails.emaillist[0], NULL,
					     subject, 1, maildata);
		free(listaddr);
		free(listname);
		free(listfqdn);
		free(fromstr);
		free(subject);
		execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", fromemails.emaillist[0],
				"-F", fromaddr,
				"-m", queuefilename, 0);

		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
		exit(EXIT_FAILURE);
	}

	subonlypost = statctrl(listdir, "subonlypost");
	if(subonlypost) {
		if(is_subbed(listdir, fromemails.emaillist[0]) != 0) {
			listname = genlistname(listaddr);
			listfqdn = genlistfqdn(listaddr);
			maildata[0] = "*LSTADDR*";
			maildata[1] = listaddr;
			maildata[2] = "*POSTERADDR*";
			maildata[3] = fromemails.emaillist[0];
			fromaddr = concatstr(3, listname, "+bounces-help@",
					listfqdn);
			fromstr = concatstr(3, listname, "+owner@", listfqdn);
			subject = concatstr(3, "Post to ", listaddr,
						" denied");
			queuefilename = prepstdreply(listdir, "subonlypost",
					fromstr, fromemails.emaillist[0], NULL,
					     subject, 2, maildata);
			free(listaddr);
			free(listname);
			free(listfqdn);
			free(fromstr);
			free(subject);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "1",
					"-T", fromemails.emaillist[0],
					"-F", fromaddr,
					"-m", queuefilename, 0);

			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
			exit(EXIT_FAILURE);
		}
	}

	moderated = statctrl(listdir, "moderated");

	if(moderated) {
		mqueuename = concatstr(3, listdir, "/moderation/",
				       randomstr);
		free(randomstr);
		if(rename(donemailname, mqueuename) < 0) {
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
