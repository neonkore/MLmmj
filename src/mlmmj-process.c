/* Copyright (C) 2003, 2003, 2004 Mads Martin Joergensen <mmj at mmj.dk>
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
#include "getlistdelim.h"
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
#include "log_oper.h"
#include "chomp.h"

enum action {
	ALLOW,
	DENY,
	MODERATE
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
	char *from, *listfqdn, *listname, *moderators = NULL;
	char *buf, *replyto, *listaddr = getlistaddr(listdir), *listdelim;
	char *queuefilename = NULL, *moderatorsfilename;
	char *mailbasename = mybasename(mailfilename), *tmp, *to;
	int queuefd, moderatorsfd, mailfd;
	size_t count = 0;
	char *maildata[4] = { "moderateaddr", NULL, "moderators", NULL };
#if 0
	printf("mailfilename = [%s], mailbasename = [%s]\n", mailfilename,
			                                     mailbasename);
#endif
	listfqdn = genlistfqdn(listaddr);
	listname = genlistname(listaddr);

	if((mailfd = open(mailfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", mailfilename);
		exit(EXIT_FAILURE);
	}

	moderatorsfilename = concatstr(2, listdir, "/control/moderators");
	if((moderatorsfd = open(moderatorsfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", moderatorsfilename);
		myfree(moderatorsfilename);
		exit(EXIT_FAILURE);
	}
	myfree(moderatorsfilename);

	while((buf = mygetline(moderatorsfd))) {
		tmp = moderators;
		moderators = concatstr(2, moderators, buf);
		myfree(buf);
		myfree(tmp);
	}

	close(moderatorsfd);

	listdelim = getlistdelim(listdir);
	replyto = concatstr(6, listname, listdelim, "moderate-", mailbasename,
			    "@", listfqdn);

	maildata[1] = replyto;
	maildata[3] = moderators;

	from = concatstr(4, listname, listdelim, "owner@", listfqdn);
	to = concatstr(3, listname, "-moderators@", listfqdn); /* FIXME JFA: Should this be converted? Why, why not? */

	myfree(listdelim);
	myfree(listname);
	myfree(listfqdn);

	queuefilename = prepstdreply(listdir, "moderation", "$listowner$",
				     to, replyto, 2, maildata);

	if((queuefd = open(queuefilename, O_WRONLY|O_APPEND)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		myfree(queuefilename);
		exit(EXIT_FAILURE);
	}
	
	while(count < 100 && (buf = mygetline(mailfd))) {
		tmp = concatstr(2, " ", buf);
		myfree(buf);
		if(writen(queuefd, tmp, strlen(tmp)) < 0)
			log_error(LOG_ARGS, "Could not write line for "
					    "moderatemail");
		myfree(tmp);
		count++;
	}
	close(queuefd);
	close(mailfd);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "2",
				"-L", listdir,
				"-F", from,
				"-m", queuefilename, (char *)NULL);

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
		} else if (strncmp(rule_ptr, "moderate", 8) == 0) {
			rule_ptr += 8;
			new_rule->act = MODERATE;
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

		/* remove unanchored ".*" from beginning of regexp to stop the
		 * regexp matching to loop so long time it seems like it's
		 * hanging */
		
		if (strlen(rule_ptr) > 2 && !strncmp(rule_ptr, ".*", 2))
			memmove(rule_ptr, rule_ptr + 2, strlen(rule_ptr) - 1);

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
			char *logstr;

			errno = 0;
			switch(rule->act) {
				case ALLOW:
					logstr = "allowed";
					break;
				case MODERATE:
					logstr = "moderated";
					break;
				default:
				case DENY:
					logstr = "denied";
					break;
			}

			log_error(LOG_ARGS, "A mail was %s by rule #%d",
					logstr, rule_nr);
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
	int i, j, opt, noprocess = 0, moderated = 0;
	int hdrfd, footfd, rawmailfd, donemailfd;
	int subonlypost = 0, addrtocc = 1, intocc = 0;
	int notoccdenymails = 0, noaccessdenymails = 0, nosubonlydenymails = 0;
	char *listdir = NULL, *mailfile = NULL, *headerfilename = NULL;
	char *footerfilename = NULL, *donemailname = NULL;
	char *randomstr = NULL, *mqueuename;
	char *mlmmjsend, *mlmmjsub, *mlmmjunsub, *mlmmjbounce;
	char *bindir, *subjectprefix, *discardname, *listaddr, *listdelim;
	char *listfqdn, *listname, *fromaddr;
	char *queuefilename, *recipextra, *owner = NULL;
	char *maildata[2] = { "posteraddr", NULL };
	struct stat st;
	uid_t uid;
	struct email_container fromemails = { 0, NULL };
	struct email_container toemails = { 0, NULL };
	struct email_container ccemails = { 0, NULL };
	struct email_container efromemails = { 0, NULL };
	struct email_container dtoemails = { 0, NULL };
	struct email_container *whichto;
	struct strlist *access_rules = NULL;
	struct strlist *delheaders = NULL;
	struct strlist allheaders;
	struct strlist *alternates = NULL;
	struct mailhdr readhdrs[] = {
		{ "From:", 0, NULL },
		{ "To:", 0, NULL },
		{ "Cc:", 0, NULL },
		{ "Return-Path:", 0, NULL },
		{ "Delivered-To:", 0, NULL },
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

	/* Lets make sure no random user tries to send mail to the list */
	if(listdir) {
		if(stat(listdir, &st) == 0) {
			uid = getuid();
			if(uid && uid != st.st_uid) {
				log_error(LOG_ARGS,
					"Have to invoke either as root "
					"or as the user owning listdir");
				writen(STDERR_FILENO,
					"Have to invoke either as root "
					"or as the user owning listdir\n", 60);
				exit(EXIT_FAILURE);
			}
		} else {
			log_error(LOG_ARGS, "Could not stat %s", listdir);
			exit(EXIT_FAILURE);
		}
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
		unlink(donemailname);
		myfree(donemailname);
		log_error(LOG_ARGS, "could not open() input mail file");
		exit(EXIT_FAILURE);
	}

    /* hdrfd is checked in do_all_the_voodo_here(), because the
     * customheaders file might not exist */
	headerfilename = concatstr(2, listdir, "/control/customheaders");
	hdrfd = open(headerfilename, O_RDONLY);
	myfree(headerfilename);
	
    /* footfd is checked in do_all_the_voodo_here(), see above */
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
	delheaders->strs[delheaders->count] = NULL;
	
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

	close(rawmailfd);
	close(donemailfd);

	if(hdrfd >= 0)
		close(hdrfd);
	if(footfd >= 0)
		close(footfd);

	if(readhdrs[0].token) { /* From: addresses */
		for(i = 0; i < readhdrs[0].valuecount; i++) {
			find_email_adr(readhdrs[0].values[i], &fromemails);
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

	if(readhdrs[3].token) { /* Return-Path: (envelope from) */
		for(i = 0; i < readhdrs[3].valuecount; i++) {
			find_email_adr(readhdrs[3].values[i], &efromemails);
		}
		if(efromemails.emailcount == 0) {
			efromemails.emaillist =
				(char **)mymalloc(sizeof(char *));
			efromemails.emaillist[0] = mystrdup("<>");
		}
	}

	if(readhdrs[4].token) { /* Delivered-To: (envelope to) */
		for(i = 0; i < readhdrs[4].valuecount; i++) {
			find_email_adr(readhdrs[4].values[i], &dtoemails);
		}
	}

	if(dtoemails.emaillist)
		whichto = &dtoemails;
	else if(toemails.emaillist)
		whichto = &toemails;
	else
		whichto = NULL;

	listdelim = getlistdelim(listdir);
	if(whichto && whichto->emaillist && whichto->emaillist[0]){
		recipextra = strstr(whichto->emaillist[0], listdelim);
		if (recipextra)
			recipextra += strlen(listdelim);
	} else
		recipextra = NULL;
	myfree(listdelim);

	if(recipextra) {
		owner = concatstr(2, listdir, "/control/owner");
		if(owner && strncmp(recipextra, "owner@", 6) == 0) {
			/* strip envelope from before resending */
			delheaders->count = 0;
			delheaders->strs = NULL;
			delheaders->strs = myrealloc(delheaders->strs,
				(delheaders->count+3) * sizeof(char *));
			delheaders->strs[delheaders->count++] =
				mystrdup("From ");
			delheaders->strs[delheaders->count++] =
				mystrdup("Return-Path:");
			delheaders->strs[delheaders->count] = NULL;
			if((rawmailfd = open(mailfile, O_RDONLY)) < 0) {
				log_error(LOG_ARGS, "could not open() "
						    "input mail file");
				exit(EXIT_FAILURE);
			}
			if((donemailfd = open(donemailname,
						O_WRONLY|O_TRUNC)) < 0) {
				log_error(LOG_ARGS, "could not open() "
						    "output mail file");
				exit(EXIT_FAILURE);
			}
			if(do_all_the_voodo_here(rawmailfd, donemailfd, -1,
					-1, (const char**)delheaders->strs,
					NULL, &allheaders, NULL) < 0) {
				log_error(LOG_ARGS, "do_all_the_voodo_here");
				exit(EXIT_FAILURE);
			}
			close(rawmailfd);
			close(donemailfd);
			unlink(mailfile);
			log_oper(listdir, OPLOGFNAME, "mlmmj-recieve: sending"
					" mail from %s to owner",
					efromemails.emaillist[0]);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "4",
					"-L", listdir,
					"-F", efromemails.emaillist[0],
					"-s", owner,
					"-a",
					"-m", donemailname, (char *)NULL);
			log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsend);
			exit(EXIT_FAILURE);
		}
#if 0
		log_error(LOG_ARGS, "listcontrol(from, %s, %s, %s, %s, %s, %s)\n", listdir, toemails.emaillist[0], mlmmjsub, mlmmjunsub, mlmmjsend, mlmmjbounce);
#endif
		unlink(mailfile);
		listcontrol(&fromemails, listdir, whichto->emaillist[0],
			    mlmmjsub, mlmmjunsub, mlmmjsend, mlmmjbounce,
			    donemailname);
		return EXIT_SUCCESS;
	}

	/* discard malformed mail with invalid From: */
	if(fromemails.emailcount != 1) { 
		for(i = 0; i < fromemails.emailcount; i++)
			printf("fromemails.emaillist[%d] = %s\n",
					i, fromemails.emaillist[i]);
		discardname = concatstr(3, listdir,
				"/queue/discarded/", randomstr);
		log_error(LOG_ARGS, "Discarding %s due to invalid From:",
				mailfile);
		for(i = 0; i < fromemails.emailcount; i++)
			log_error(LOG_ARGS, "fromemails.emaillist[%d] = %s\n",
					i, fromemails.emaillist[i]);
		rename(mailfile, discardname);
		unlink(donemailname);
		myfree(donemailname);
		myfree(discardname);
		myfree(randomstr);
		/* TODO: free emailstructs */
		exit(EXIT_SUCCESS);
	}

	myfree(delheaders);

	if(efromemails.emailcount != 1) { /* don't send mails with <> in From
					     to the list */
		discardname = concatstr(3, listdir,
				"/queue/discarded/",
				randomstr);
		log_error(LOG_ARGS, "Discarding %s due to invalid envelope"
				" from email count (was %d, must be 1)",
				mailfile, efromemails.emailcount);
		rename(mailfile, discardname);
		unlink(donemailname);
		myfree(donemailname);
		myfree(discardname);
		myfree(randomstr);
		/* TODO: free emailstructs */
		exit(EXIT_SUCCESS);
	}

	unlink(mailfile);

	listaddr = getlistaddr(listdir);
	alternates = ctrlvalues(listdir, "listaddress");

	addrtocc = !(statctrl(listdir, "tocc"));
	if(addrtocc) {
		for(i = 0; i < toemails.emailcount; i++) {
			for(j = 0; j < alternates->count; j++) {
				chomp(alternates->strs[j]);
				if(strcasecmp(alternates->strs[j],
					toemails.emaillist[i]) == 0)
					intocc = 1;
			}
		}
		for(i = 0; i < ccemails.emailcount; i++) {
			for(j = 0; j < alternates->count; j++) {
				chomp(alternates->strs[j]);
				if(strcasecmp(alternates->strs[j],
					ccemails.emaillist[i]) == 0)
					intocc = 1;
			}
		}
	}

	for(i = 0; i < alternates->count; i++)
		myfree(alternates->strs[i]);

	notoccdenymails = statctrl(listdir, "notoccdenymails");
	
	if(addrtocc && !intocc) {
		/* Don't send a mail about denial to the list, but silently
		 * discard and exit. Also don't in case of it being turned off
		 */
		if ((strcasecmp(listaddr, fromemails.emaillist[0]) == 0) ||
				notoccdenymails) {
			log_error(LOG_ARGS, "Discarding %s because list"
					" address was not in To: or Cc:,"
					" and From: was the list or"
					" notoccdenymails was set",
					mailfile);
			myfree(listaddr);
			unlink(donemailname);
			myfree(donemailname);
			exit(EXIT_SUCCESS);
		}
		listdelim = getlistdelim(listdir);
		listname = genlistname(listaddr);
		listfqdn = genlistfqdn(listaddr);
		fromaddr = concatstr(4, listname, listdelim, "bounces-help@",
				     listfqdn);
		queuefilename = prepstdreply(listdir, "notintocc",
					"$listowner$", fromemails.emaillist[0],
					NULL, 0, NULL);
		MY_ASSERT(queuefilename)
		myfree(listdelim);
		myfree(listname);
		myfree(listfqdn);
		unlink(donemailname);
		myfree(donemailname);
		execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-L", listdir,
				"-T", fromemails.emaillist[0],
				"-F", fromaddr,
				"-m", queuefilename, (char *)NULL);

		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
		exit(EXIT_FAILURE);
	}

	nosubonlydenymails = statctrl(listdir, "nosubonlydenymails");

	subonlypost = statctrl(listdir, "subonlypost");
	if(subonlypost) {
		/* Don't send a mail about denial to the list, but silently
		 * discard and exit. */
		if (strcasecmp(listaddr, fromemails.emaillist[0]) == 0) {
			log_error(LOG_ARGS, "Discarding %s because"
					" subonlypost was set and From: was"
					" the list address",
					mailfile);
			myfree(listaddr);
			unlink(donemailname);
			myfree(donemailname);
			exit(EXIT_SUCCESS);
		}
		if(is_subbed(listdir, fromemails.emaillist[0]) != 0) {
			if(nosubonlydenymails) {
				log_error(LOG_ARGS, "Discarding %s because"
						" subonlypost and"
						" nosubonlydenymails was set",
						mailfile);
				myfree(listaddr);
				unlink(donemailname);
				myfree(donemailname);
				exit(EXIT_SUCCESS);
			}
			listdelim = getlistdelim(listdir);
			listname = genlistname(listaddr);
			listfqdn = genlistfqdn(listaddr);
			maildata[1] = fromemails.emaillist[0];
			fromaddr = concatstr(4, listname, listdelim,
					"bounces-help@", listfqdn);
			queuefilename = prepstdreply(listdir, "subonlypost",
					"$listowner$", fromemails.emaillist[0],
					NULL, 1, maildata);
			MY_ASSERT(queuefilename)
			myfree(listaddr);
			myfree(listdelim);
			myfree(listname);
			myfree(listfqdn);
			unlink(donemailname);
			myfree(donemailname);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "1",
					"-T", fromemails.emaillist[0],
					"-F", fromaddr,
					"-m", queuefilename, (char *)NULL);

			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
			exit(EXIT_FAILURE);
		}
	}

	noaccessdenymails = statctrl(listdir, "noaccessdenymails");

	access_rules = ctrlvalues(listdir, "access");
	if (access_rules) {
		enum action accret;
		/* Don't send a mail about denial to the list, but silently
		 * discard and exit. Also do this in case it's turned off */
		accret = do_access(access_rules, &allheaders);
		if (accret == DENY) {
			if ((strcasecmp(listaddr, fromemails.emaillist[0]) ==
						0) || noaccessdenymails) {
				log_error(LOG_ARGS, "Discarding %s because"
						" it was denied by an access"
						" rule, and From: was the list"
						" address or noaccessdenymails"
						" was set",
						mailfile);
				myfree(listaddr);
				unlink(donemailname);
				myfree(donemailname);
				exit(EXIT_SUCCESS);
			}
			listdelim = getlistdelim(listdir);
			listname = genlistname(listaddr);
			listfqdn = genlistfqdn(listaddr);
			fromaddr = concatstr(4, listname, listdelim,
					"bounces-help@", listfqdn);
			queuefilename = prepstdreply(listdir, "access",
							"$listowner$",
							fromemails.emaillist[0],
							NULL, 0, NULL);
			MY_ASSERT(queuefilename)
			myfree(listaddr);
			myfree(listdelim);
			myfree(listname);
			myfree(listfqdn);
			unlink(donemailname);
			myfree(donemailname);
			myfree(randomstr);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "1",
					"-L", listdir,
					"-T", fromemails.emaillist[0],
					"-F", fromaddr,
					"-m", queuefilename, (char *)NULL);

			log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsend);
			exit(EXIT_FAILURE);
		} else if (accret == MODERATE) {
			moderated = 1;
		}
	}
	
	if(!moderated)
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
				"-m", donemailname, (char *)NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);

	return EXIT_FAILURE;
}
