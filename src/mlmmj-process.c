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
#include <regex.h>

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
#include "memory.h"


enum action {
	ALLOW,
	DENY
};


struct rule_list {
	regex_t regexp;
	unsigned int not;
	enum action act;
	struct rule_list *next;
};


void newmoderated(const char *listdir, const char *mailfilename,
		  const char *mlmmjsend)
{
	char *from, *fqdn, *listname;
	char *buf, *moderatorfilename, *listaddr = getlistaddr(listdir);
	char *queuefilename = NULL, *moderatorsfilename, *randomstr = NULL;
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
		myfree(moderatorfilename);
		exit(EXIT_FAILURE);
	}
	myfree(moderatorfilename);

	moderatorsfilename = concatstr(2, listdir, "/control/moderators");
	if((moderatorsfd = open(moderatorsfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", moderatorsfilename);
		myfree(moderatorsfilename);
		close(queuefd);
		exit(EXIT_FAILURE);
	}
	myfree(moderatorsfilename);

	if((mailfd = open(mailfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", mailfilename);
		close(queuefd);
		exit(EXIT_FAILURE);
	}

        do {
                randomstr = random_str();
                myfree(queuefilename);
                queuefilename = concatstr(3, listdir, "/queue/", randomstr);
                myfree(randomstr);

                queuefd = open(queuefilename, O_RDWR|O_CREAT|O_EXCL,
					      S_IRUSR|S_IWUSR);

        } while ((queuefd < 0) && (errno == EEXIST));

	if(queuefd < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		myfree(queuefilename);
		exit(EXIT_FAILURE);
	}

	from = concatstr(3, listname, "+owner@", fqdn);
	s1 = concatstr(15, "From: ", from, "\nTo: ", listname, "-moderators@",
			fqdn, "\nReply-To: ", listname,	"+moderate-",
			mailbasename, "@", fqdn,
			"\nSubject: Moderation needed for ", listaddr, "\n\n");
	if(writen(queuefd, s1, strlen(s1)) < 0) {
		log_error(LOG_ARGS, "Could not write to %s", queuefilename);
		exit(EXIT_FAILURE);
	}
	myfree(s1);
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
			myfree(s1);
		} else if(strncmp(buf, "*MODERATORS*", 12) == 0) {
			myfree(buf);
			while((buf = mygetline(moderatorsfd))) {
				if(writen(queuefd, buf, strlen(buf)) < 0)
					log_error(LOG_ARGS,
						"Could not write moderators");
					
				myfree(buf);
				buf = NULL;
			}
		} else
			if(writen(queuefd, buf, strlen(buf)) < 0) {
				log_error(LOG_ARGS,
					"Could not write moderatemail");
				exit(EXIT_FAILURE);
			}
		myfree(buf);
	}
	close(moderatorfd);
	close(moderatorsfd);
	while((buf = mygetline(mailfd)) && count < 100) {
		s1 = concatstr(2, " ", buf);
		myfree(buf);
		writen(queuefd, s1, strlen(s1));
		myfree(s1);
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


static void free_rules(struct rule_list *rule)
{
	struct rule_list *next;

	while (rule) {
		next = rule->next;
		regfree(&rule->regexp);
		myfree(rule);
		rule = next;
	}
}


static enum action do_access(struct strlist *rule_strs, struct strlist *hdrs)
{
	int i;
	unsigned int match;
	unsigned int rule_nr;
	struct rule_list *head = NULL;
	struct rule_list *rule, *new_rule;
	char *rule_ptr;
	char errbuf[128];
	int err;
	enum action ret;

	/* They're going in backwards later on, so loop from the end here
	 * to get it right
	 */
	for (i=rule_strs->count-1; i>=0; i--) {
		new_rule = mymalloc(sizeof(struct rule_list));

		/* linked list */
		new_rule->next = head;
		head = new_rule;

		rule_ptr = rule_strs->strs[i];
		if (strncmp(rule_ptr, "allow", 5) == 0) {
			rule_ptr += 5;
			new_rule->act = ALLOW;
		} else if (strncmp(rule_ptr, "deny", 4) == 0) {
			rule_ptr += 4;
			new_rule->act = DENY;
		} else {
			errno = 0;
			log_error(LOG_ARGS, "Unable to parse rule #%d!"
					" Denying post to list", i);
			free_rules(head);
			return DENY;
		}

		if (*rule_ptr == ' ') {
			rule_ptr++;
		} else if (*rule_ptr == '\0') {
			/* We had a single allow/deny so match everything */
			*(--rule_ptr) = '.';
		} else {
			/* we must have space or end of string */
			errno = 0;
			log_error(LOG_ARGS, "Unable to parse rule #%d!"
					" Denying post to list", i);
			free_rules(head);
			return DENY;
		}

		if (*rule_ptr == '!') {
			rule_ptr++;
			new_rule->not = 1;
		} else {
			new_rule->not = 0;
		}

		if ((err = regcomp(&new_rule->regexp, rule_ptr,
				REG_EXTENDED | REG_NOSUB | REG_ICASE))) {
			regerror(err, &new_rule->regexp, errbuf,
					sizeof(errbuf));
			errno = 0;
			log_error(LOG_ARGS, "regcomp() failed for rule #%d!"
					" (message: '%s') (expression: '%s')"
					" Denying post to list",
					i, errbuf, rule_ptr);
			free_rules(head);
			return DENY;
		}
		
#if 0
		printf("rule #%d: %s if%s match '%s'\n", i,
				(new_rule->act == ALLOW) ? "allow" : "deny",
				(new_rule->not) ? " not" : "",
				rule_ptr);
#endif

	}

	rule = head;
	for (rule_nr=0; rule; rule_nr++) {
		match = 0;
		for (i=0; i<hdrs->count; i++) {
			if (regexec(&rule->regexp, hdrs->strs[i], 0, NULL, 0)
					== 0) {
				match = 1;
				break;
			}
		}
		if (match != rule->not) {
			errno = 0;
			log_error(LOG_ARGS, "A mail was %s by rule #%d",
					(rule->act == ALLOW) ?
					"allowed" : "denied",
					rule_nr);
			ret = rule->act;
			free_rules(head);
			return ret;
		}

		rule = rule->next;
	}

	free_rules(head);
	return DENY;
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
	int i, opt, noprocess = 0, moderated = 0;
	int hdrfd, footfd, rawmailfd, donemailfd;
	int subonlypost = 0, addrtocc = 1, intocc = 0;
	char *listdir = NULL, *mailfile = NULL, *headerfilename = NULL;
	char *footerfilename = NULL, *donemailname = NULL;
	char *randomstr = NULL, *mqueuename;
	char *mlmmjsend, *mlmmjsub, *mlmmjunsub, *mlmmjbounce;
	char *bindir, *subjectprefix, *discardname, *listaddr;
	char *listfqdn, *listname, *fromaddr, *fromstr, *subject;
	char *queuefilename, *recipdelim, *owner = NULL;
	char *maildata[4];
	struct email_container fromemails = { 0, NULL };
	struct email_container toemails = { 0, NULL };
	struct email_container ccemails = { 0, NULL };
	struct email_container efromemails = { 0, NULL };
	struct strlist *access_rules = NULL;
	struct strlist *delheaders = NULL;
	struct strlist allheaders;
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
	myfree(bindir);
	
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

        do {
                myfree(donemailname);
                myfree(randomstr);
                randomstr = random_str();
                donemailname = concatstr(3, listdir, "/queue/", randomstr);

                donemailfd = open(donemailname, O_RDWR|O_CREAT|O_EXCL,
						S_IRUSR|S_IWUSR);

        } while ((donemailfd < 0) && (errno == EEXIST));
	
	if(donemailfd < 0) {
		log_error(LOG_ARGS, "could not create %s", donemailname);
		myfree(donemailname);
		exit(EXIT_FAILURE);
	}
#if 0
	log_error(LOG_ARGS, "donemailname = [%s]\n", donemailname);
#endif
	if((rawmailfd = open(mailfile, O_RDONLY)) < 0) {
		myfree(donemailname);
		log_error(LOG_ARGS, "could not open() input mail file");
		exit(EXIT_FAILURE);
	}

	headerfilename = concatstr(2, listdir, "/control/customheaders");
	hdrfd = open(headerfilename, O_RDONLY);
	myfree(headerfilename);
	
	footerfilename = concatstr(2, listdir, "/control/footer");
	footfd = open(footerfilename, O_RDONLY);
	myfree(footerfilename);

	delheaders = ctrlvalues(listdir, "delheaders");
	if(delheaders == NULL) {
		delheaders = mymalloc(sizeof(struct strlist));
		delheaders->count = 0;
		delheaders->strs = NULL;
	}

	delheaders->strs = myrealloc(delheaders->strs,
			(delheaders->count+3) * sizeof(char *));
	delheaders->strs[delheaders->count++] = mystrdup("From ");
	delheaders->strs[delheaders->count++] = mystrdup("Return-Path:");
	delheaders->strs[delheaders->count++] = NULL;
	
	subjectprefix = ctrlvalue(listdir, "prefix");	
	
	if(do_all_the_voodo_here(rawmailfd, donemailfd, hdrfd, footfd,
				(const char**)delheaders->strs, readhdrs,
				&allheaders, subjectprefix) < 0) {
		log_error(LOG_ARGS, "Error in do_all_the_voodo_here");
		exit(EXIT_FAILURE);
	}

	for(i = 0; i < delheaders->count; i++)
		myfree(delheaders->strs[i]);
	myfree(delheaders->strs);
	myfree(delheaders);

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
			myfree(donemailname);
			myfree(discardname);
			myfree(randomstr);
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

	if(toemails.emaillist)
		recipdelim = strchr(toemails.emaillist[0], RECIPDELIM);
	else
		recipdelim = NULL;

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
		MY_ASSERT(queuefilename)
		myfree(listaddr);
		myfree(listname);
		myfree(listfqdn);
		myfree(fromstr);
		myfree(subject);
		unlink(donemailname);
		myfree(donemailname);
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
			MY_ASSERT(queuefilename)
			myfree(listaddr);
			myfree(listname);
			myfree(listfqdn);
			myfree(fromstr);
			myfree(subject);
			unlink(donemailname);
			myfree(donemailname);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "1",
					"-T", fromemails.emaillist[0],
					"-F", fromaddr,
					"-m", queuefilename, 0);

			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
			exit(EXIT_FAILURE);
		}
	}

	access_rules = ctrlvalues(listdir, "access");
	if (access_rules) {
		if (do_access(access_rules, &allheaders) == DENY) {
			listname = genlistname(listaddr);
			listfqdn = genlistfqdn(listaddr);
			maildata[0] = "*LSTADDR*";
			maildata[1] = listaddr;
			fromaddr = concatstr(3, listname, "+bounces-help@",
					listfqdn);
			fromstr = concatstr(3, listname, "+owner@", listfqdn);
			subject = concatstr(3, "Post to ", listaddr,
					" denied.");
			queuefilename = prepstdreply(listdir, "access",
							fromstr,
							fromemails.emaillist[0],
							NULL,
							subject, 1, maildata);
			MY_ASSERT(queuefilename)
			myfree(listaddr);
			myfree(listname);
			myfree(listfqdn);
			myfree(fromstr);
			myfree(subject);
			unlink(donemailname);
			myfree(donemailname);
			myfree(randomstr);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "1",
					"-T", fromemails.emaillist[0],
					"-F", fromaddr,
					"-m", queuefilename, 0);

			log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsend);
			exit(EXIT_FAILURE);
		}
	}
	

	moderated = statctrl(listdir, "moderated");
	if(moderated) {
		mqueuename = concatstr(3, listdir, "/moderation/",
				       randomstr);
		myfree(randomstr);
		if(rename(donemailname, mqueuename) < 0) {
			log_error(LOG_ARGS, "could not rename(%s,%s)",
					    donemailname, mqueuename);
			myfree(donemailname);
			myfree(mqueuename);
			exit(EXIT_FAILURE);
		}
		myfree(donemailname);
		newmoderated(listdir, mqueuename, mlmmjsend);
		return EXIT_SUCCESS;
	}

	myfree(randomstr);

	if(noprocess) {
		myfree(donemailname);
		/* XXX: toemails and ccemails etc. have to be myfree() */
		exit(EXIT_SUCCESS);
	}
	
	execlp(mlmmjsend, mlmmjsend,
				"-L", listdir,
				"-m", donemailname, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);

	return EXIT_FAILURE;
}
