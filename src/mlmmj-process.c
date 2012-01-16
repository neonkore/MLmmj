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
#include "do_all_the_voodoo_here.h"
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
#include "unistr.h"

enum action {
	ALLOW,
	SEND,
	DENY,
	MODERATE,
	DISCARD
};


static char *action_strs[] = {
	"allowed",
	"sent",
	"denied",
	"moderated",
	"discarded"
};


enum modreason {
	MODNONSUBPOSTS,
	ACCESS,
	MODERATED
};


static char *modreason_strs[] = {
	"modnonsubposts",
	"access",
	"moderated"
};


static void newmoderated(const char *listdir, const char *mailfilename,
		  const char *mlmmjsend, const char *efromsender,
		  const char *subject, const char *posteraddr,
		  enum modreason modreason)
{
	char *from, *listfqdn, *listname, *moderators = NULL;
	char *buf, *replyto, *listaddr = getlistaddr(listdir), *listdelim;
	text *txt;
	char *queuefilename = NULL, *moderatorsfilename, *efromismod = NULL;
	char *mailbasename = mybasename(mailfilename), *tmp, *to;
	int moderatorsfd, foundaddr = 0, notifymod = 0, status;
	pid_t childpid, pid;
#if 0
	printf("mailfilename = [%s], mailbasename = [%s]\n", mailfilename,
			                                     mailbasename);
#endif
	listfqdn = genlistfqdn(listaddr);
	listname = genlistname(listaddr);

	moderatorsfilename = concatstr(2, listdir, "/control/moderators");
	if((moderatorsfd = open(moderatorsfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", moderatorsfilename);
		myfree(moderatorsfilename);
		exit(EXIT_FAILURE);
	}
	myfree(moderatorsfilename);

	if(statctrl(listdir, "ifmodsendonlymodmoderate"))
		efromismod = concatstr(2, efromsender, "\n");

	while((buf = mygetline(moderatorsfd))) {
		if(efromismod && strcmp(buf, efromismod) == 0)
			foundaddr = 1;
		tmp = moderators;
		moderators = concatstr(2, moderators, buf);
		myfree(buf);
		myfree(tmp);
	}

	if(!foundaddr) {
		myfree(efromismod);
		efromismod = NULL;
	}

	close(moderatorsfd);

	listdelim = getlistdelim(listdir);
	replyto = concatstr(6, listname, listdelim, "moderate-", mailbasename,
			    "@", listfqdn);

	from = concatstr(4, listname, listdelim, "owner@", listfqdn);
	to = concatstr(3, listname, "-moderators@", listfqdn); /* FIXME JFA: Should this be converted? Why, why not? */

	myfree(listdelim);
	myfree(listname);
	myfree(listfqdn);

	txt = open_text(listdir, "moderate", "post",
			modreason_strs[modreason], NULL, "moderation");
	MY_ASSERT(txt);
	register_unformatted(txt, "subject", subject);
	register_unformatted(txt, "posteraddr", posteraddr);
	register_unformatted(txt, "moderateaddr", replyto); /* DEPRECATED */
	register_unformatted(txt, "releaseaddr", replyto);
	if(efromismod) register_unformatted(txt, "moderators", efromismod);
	else register_unformatted(txt, "moderators", moderators);
	register_originalmail(txt, mailfilename);
	queuefilename = prepstdreply(txt, listdir, "$listowner$", to, replyto);
	MY_ASSERT(queuefilename);
	close_text(txt);

	/* we might need to exec more than one mlmmj-send */
	
	notifymod = !efromismod && statctrl(listdir,"notifymod");
	
	if (notifymod) {
		childpid = fork();
		if(childpid < 0)
			log_error(LOG_ARGS, "Could not fork; poster not notified");
	} else
		childpid = -1;

	if(childpid != 0) {
		if(childpid > 0) {
			do /* Parent waits for the child */
				pid = waitpid(childpid, &status, 0);
			while(pid == -1 && errno == EINTR);
		}
		if(efromismod)
			execlp(mlmmjsend, mlmmjsend,
					"-l", "1",
					"-L", listdir,
					"-F", from,
					"-m", queuefilename,
					"-T", efromsender, (char *)NULL);
		else
			execlp(mlmmjsend, mlmmjsend,
					"-l", "2",
					"-L", listdir,
					"-F", from,
					"-m", queuefilename, (char *)NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
		exit(EXIT_FAILURE);
	}

	myfree(queuefilename);

	/* send mail to poster that the list is moderated */

	txt = open_text(listdir, "wait", "post",
			modreason_strs[modreason], NULL, "moderation-poster");
	MY_ASSERT(txt);
	register_unformatted(txt, "subject", subject);
	register_unformatted(txt, "posteraddr", posteraddr);
	if(efromismod) register_unformatted(txt, "moderators", efromismod);
	else register_unformatted(txt, "moderators", moderators);
	register_originalmail(txt, mailfilename);
	queuefilename = prepstdreply(txt, listdir,
			"$listowner$", efromsender, NULL);
	MY_ASSERT(queuefilename);
	close_text(txt);

	execlp(mlmmjsend, mlmmjsend,
			"-l", "1",
			"-L", listdir,
			"-F", from,
			"-m", queuefilename,
			"-T", efromsender, (char *)NULL);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}


static enum action do_access(struct strlist *rule_strs, struct strlist *hdrs,
		const char *from, const char *listdir)
{
	int i, j;
	unsigned int match;
	char *rule_ptr;
	char errbuf[128];
	int err;
	enum action act;
	unsigned int not;
	regex_t regexp;
	char *hdr;

	for (i=0; i<rule_strs->count; i++) {

		rule_ptr = rule_strs->strs[i];

		if (strncmp(rule_ptr, "allow", 5) == 0) {
			rule_ptr += 5;
			act = ALLOW;
		} else if (strncmp(rule_ptr, "send", 4) == 0) {
			rule_ptr += 4;
			act = SEND;
		} else if (strncmp(rule_ptr, "deny", 4) == 0) {
			rule_ptr += 4;
			act = DENY;
		} else if (strncmp(rule_ptr, "moderate", 8) == 0) {
			rule_ptr += 8;
			act = MODERATE;
		} else if (strncmp(rule_ptr, "discard", 7) == 0) {
			rule_ptr += 7;
			act = DISCARD;
		} else {
			errno = 0;
			log_error(LOG_ARGS, "Unable to parse rule #%d \"%s\":"
					" Missing action keyword. Denying post from \"%s\"",
					i, rule_strs->strs[i], from);
			log_oper(listdir, OPLOGFNAME, "Unable to parse rule #%d \"%s\":"
					" Missing action keyword. Denying post from \"%s\"",
					i, rule_strs->strs[i], from);
			return DENY;
		}

		if (*rule_ptr == ' ') {
			rule_ptr++;
		} else if (*rule_ptr == '\0') {
			/* the rule is a keyword and no regexp */
			log_oper(listdir, OPLOGFNAME, "mlmmj-process: access -"
					" A mail from \"%s\" was %s by rule #%d \"%s\"",
					from, action_strs[act], i, rule_strs->strs[i]);
			return act;
		} else {
			/* we must have space or end of string */
			errno = 0;
			log_error(LOG_ARGS, "Unable to parse rule #%d \"%s\":"
					" Invalid character after action keyword."
					" Denying post from \"%s\"", i, rule_strs->strs[i], from);
			log_oper(listdir, OPLOGFNAME, "Unable to parse rule #%d \"%s\":"
					" Invalid character after action keyword."
					" Denying post from \"%s\"", i, rule_strs->strs[i], from);
			return DENY;
		}

		if (*rule_ptr == '!') {
			rule_ptr++;
			not = 1;
		} else {
			not = 0;
		}

		/* remove unanchored ".*" from beginning of regexp to stop the
		 * regexp matching to loop so long time it seems like it's
		 * hanging */
		if (strncmp(rule_ptr, "^.*", 3) == 0) {
			rule_ptr += 3;
		}
		while (strncmp(rule_ptr, ".*", 2) == 0) {
			rule_ptr += 2;
		}

		if ((err = regcomp(&regexp, rule_ptr,
				REG_EXTENDED | REG_NOSUB | REG_ICASE))) {
			regerror(err, &regexp, errbuf, sizeof(errbuf));
			regfree(&regexp);
			errno = 0;
			log_error(LOG_ARGS, "regcomp() failed for rule #%d \"%s\""
					" (message: '%s') (expression: '%s')"
					" Denying post from \"%s\"",
					i, rule_strs->strs[i], errbuf, rule_ptr, from);
			log_oper(listdir, OPLOGFNAME, "regcomp() failed for rule"
					" #%d \"%s\" (message: '%s') (expression: '%s')"
					" Denying post from \"%s\"",
					i, rule_strs->strs[i], errbuf, rule_ptr, from);
			return DENY;
		}

		match = 0;
		for (j=0; j<hdrs->count; j++) {
			if (regexec(&regexp, hdrs->strs[j], 0, NULL, 0)
					== 0) {
				match = 1;
				break;
			}
		}

		regfree(&regexp);

		if (match != not) {
			if (match) {
				hdr = mystrdup(hdrs->strs[j]);
				chomp(hdr);
				log_oper(listdir, OPLOGFNAME, "mlmmj-process: access -"
						" A mail from \"%s\" with header \"%s\" was %s by"
						" rule #%d \"%s\"", from, hdr, action_strs[act],
						i, rule_strs->strs[i]);
				myfree(hdr);
			} else {
				log_oper(listdir, OPLOGFNAME, "mlmmj-process: access -"
						" A mail from \"%s\" was %s by rule #%d \"%s\""
						" because no header matched.", from,
						action_strs[act], i, rule_strs->strs[i]);
			}
			return act;
		}

	}

	log_oper(listdir, OPLOGFNAME, "mlmmj-process: access -"
			" A mail from \"%s\" didn't match any rules, and"
			" was denied by default.", from);
	return DENY;
}


static char *recipient_extra(const char *listdir, const char *addr)
{
	char *listdelim;
	char *delim, *atsign, *ret;
	size_t len;

	if (!addr)
		return NULL;

	listdelim = getlistdelim(listdir);

	delim = strstr(addr, listdelim);
	if (!delim) {
		myfree(listdelim);
		return NULL;
	}
	delim += strlen(listdelim);
	myfree(listdelim);

	atsign = strrchr(delim, '@');
	if (!atsign)
		return NULL;

	len = atsign - delim;
	ret = (char *)mymalloc(len + 1);
	strncpy(ret, delim, len);
	ret[len] = '\0';

	return ret;
}


static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/list\n"
	       "       -m /path/to/mail [-h] [-P] [-V]\n"
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
	enum modreason modreason;
	int hdrfd, footfd, rawmailfd, donemailfd, omitfd;
	int subonlypost = 0, addrtocc = 1, intocc = 0, modnonsubposts = 0;
	int maxmailsize = 0;
	int notoccdenymails = 0, noaccessdenymails = 0, nosubonlydenymails = 0;
	int nomaxmailsizedenymails = 0;
	int notmetoo = 0;
	char *listdir = NULL, *mailfile = NULL, *headerfilename = NULL;
	char *footerfilename = NULL, *donemailname = NULL;
	char *randomstr = NULL, *mqueuename, *omitfilename;
	char *mlmmjsend, *mlmmjsub, *mlmmjunsub, *mlmmjbounce;
	char *bindir, *subjectprefix, *discardname, *listaddr, *listdelim;
	char *listfqdn, *listname, *fromaddr;
	text *txt;
	char *queuefilename, *recipextra = NULL, *owner = NULL;
	char *maxmailsizestr;
	char *subject = NULL, *posteraddr = NULL;
	char *envstr, *efrom;
	struct stat st;
	uid_t uid;
	struct email_container fromemails = { 0, NULL };
	struct email_container toemails = { 0, NULL };
	struct email_container ccemails = { 0, NULL };
	struct email_container rpemails = { 0, NULL };
	struct email_container dtemails = { 0, NULL };
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
		{ "Subject:", 0, NULL },
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

    /* hdrfd is checked in do_all_the_voodoo_here(), because the
     * customheaders file might not exist */
	headerfilename = concatstr(2, listdir, "/control/customheaders");
	hdrfd = open(headerfilename, O_RDONLY);
	myfree(headerfilename);

    /* footfd is checked in do_all_the_voodoo_here(), see above */
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

	if(do_all_the_voodoo_here(rawmailfd, donemailfd, hdrfd, footfd,
				(const char**)delheaders->strs, readhdrs,
				&allheaders, subjectprefix) < 0) {
		log_error(LOG_ARGS, "Error in do_all_the_voodoo_here");
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

	/* From: addresses */
	for(i = 0; i < readhdrs[0].valuecount; i++) {
		find_email_adr(readhdrs[0].values[i], &fromemails);
	}
	/* discard malformed mail with invalid From: unless it's a bounce */
	if(fromemails.emailcount != 1 &&
			(recipextra == NULL ||
			strncmp(recipextra, "bounces", 7) != 0)) {
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
	posteraddr = fromemails.emaillist[0];

	/* To: addresses */
	for(i = 0; i < readhdrs[1].valuecount; i++) {
		find_email_adr(readhdrs[1].values[i], &toemails);
	}

	/* Cc: addresses */
	for(i = 0; i < readhdrs[2].valuecount; i++) {
		find_email_adr(readhdrs[2].values[i], &ccemails);
	}

	/* Return-Path: addresses */
	for(i = 0; i < readhdrs[3].valuecount; i++) {
		find_email_adr(readhdrs[3].values[i], &rpemails);
	}

	/* Delivered-To: addresses */
	for(i = 0; i < readhdrs[4].valuecount; i++) {
		find_email_adr(readhdrs[4].values[i], &dtemails);
	}

	/* Subject: */
	if (readhdrs[5].valuecount)
			subject = unistr_header_to_utf8(readhdrs[5].values[0]);
	if (!subject) subject = mystrdup("");

	/* envelope from */
	if((envstr = getenv("SENDER")) != NULL) {
		/* qmail, postfix, exim */
		efrom = mystrdup(envstr);
	} else if(rpemails.emailcount >= 1) {
		/* the (first) Return-Path: header */
		efrom = mystrdup(rpemails.emaillist[0]);
	} else {
		efrom = mystrdup("");
	}

	/* address extension (the "foo" part of "user+foo@domain.tld") */
	if((envstr = getenv("DEFAULT")) != NULL) {
		/* qmail */
		recipextra = mystrdup(envstr);
	} else if((envstr = getenv("EXTENSION")) != NULL) {
		/* postfix */
		recipextra = mystrdup(envstr);
	} else if((envstr = getenv("LOCAL_PART_SUFFIX")) != NULL) {
		/* exim */
		listdelim = getlistdelim(listdir);
		if (strncmp(envstr, listdelim, strlen(listdelim)) == 0) {
			recipextra = mystrdup(envstr + strlen(listdelim));
		} else {
			recipextra = mystrdup(envstr);
		}
		myfree(listdelim);
	} else if(dtemails.emailcount >= 1) {
		/* parse the (first) Delivered-To: header */
		recipextra = recipient_extra(listdir, dtemails.emaillist[0]);
	} else if(toemails.emailcount >= 1) {
		/* parse the (first) To: header */
		recipextra = recipient_extra(listdir, toemails.emaillist[0]);
	} else {
		recipextra = NULL;
	}

	if(recipextra && (strlen(recipextra) == 0)) {
		myfree(recipextra);
		recipextra = NULL;
	}

	/* Why is this here, and not in listcontrol() ?  -- mortenp 20060409 */
	if(recipextra) {
		owner = concatstr(2, listdir, "/control/owner");
		if(owner && strcmp(recipextra, "owner") == 0) {
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
			if(do_all_the_voodoo_here(rawmailfd, donemailfd, -1,
					-1, (const char**)delheaders->strs,
					NULL, &allheaders, NULL) < 0) {
				log_error(LOG_ARGS, "do_all_the_voodoo_here");
				exit(EXIT_FAILURE);
			}
			close(rawmailfd);
			close(donemailfd);
			unlink(mailfile);
			log_oper(listdir, OPLOGFNAME, "mlmmj-process: sending"
					" mail from %s to owner",
					efrom);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "4",
					"-L", listdir,
					"-F", efrom,
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
		listcontrol(&fromemails, listdir, recipextra,
			    mlmmjsub, mlmmjunsub, mlmmjsend, mlmmjbounce,
			    donemailname);

		return EXIT_SUCCESS;
	}

	listaddr = getlistaddr(listdir);
	alternates = ctrlvalues(listdir, "listaddress");

	/* checking incoming mail's size */
	maxmailsizestr = ctrlvalue(listdir, "maxmailsize");
	if(maxmailsizestr) {
		maxmailsize = atol(maxmailsizestr);
		if(stat(donemailname, &st) < 0) {
			log_error(LOG_ARGS, "stat(%s,..) failed", donemailname);
			exit(EXIT_FAILURE);
		}

		if(st.st_size > maxmailsize) {

			nomaxmailsizedenymails = statctrl(listdir, "nomaxmailsizedenymails");
			if (nomaxmailsizedenymails) {
				errno = 0;
				log_error(LOG_ARGS, "Discarding %s due to"
						" size limit (%d bytes too big)",
						donemailname, (st.st_size - maxmailsize));
				unlink(donemailname);
				unlink(mailfile);
				myfree(donemailname);
				myfree(maxmailsizestr);
				exit(EXIT_SUCCESS);
			}

			listdelim = getlistdelim(listdir);
			listname = genlistname(listaddr);
			listfqdn = genlistfqdn(listaddr);
			fromaddr = concatstr(4, listname, listdelim,
					"bounces-help@", listfqdn);
			txt = open_text(listdir, "deny", "post",
					"maxmailsize", NULL, "maxmailsize");
			MY_ASSERT(txt);
			register_unformatted(txt, "subject", subject);
			register_unformatted(txt, "posteraddr", posteraddr);
			register_unformatted(txt, "maxmailsize", maxmailsizestr);
			register_originalmail(txt, donemailname);
			queuefilename = prepstdreply(txt, listdir,
					"$listowner$", posteraddr, NULL);
			MY_ASSERT(queuefilename);
			close_text(txt);
			myfree(listdelim);
			myfree(listname);
			myfree(listfqdn);
			unlink(donemailname);
			unlink(mailfile);
			myfree(donemailname);
			myfree(maxmailsizestr);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "1",
					"-L", listdir,
					"-T", posteraddr,
					"-F", fromaddr,
					"-m", queuefilename, (char *)NULL);

			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
			exit(EXIT_FAILURE);
		}
	}

	myfree(delheaders);

	if(strcmp(efrom, "") == 0) { /* don't send mails with <> in From
					     to the list */
		discardname = concatstr(3, listdir,
				"/queue/discarded/",
				randomstr);
		errno = 0;
		log_error(LOG_ARGS, "Discarding %s due to missing envelope"
				" from address", mailfile);
		rename(mailfile, discardname);
		unlink(donemailname);
		myfree(donemailname);
		myfree(discardname);
		myfree(randomstr);
		/* TODO: free emailstructs */
		exit(EXIT_SUCCESS);
	}

	unlink(mailfile);

	addrtocc = !(statctrl(listdir, "tocc"));
	if(addrtocc) {
		for(i = 0; i < toemails.emailcount; i++) {
			log_error(LOG_ARGS, "Found To: %s",
				toemails.emaillist[i]);
			for(j = 0; j < alternates->count; j++) {
				chomp(alternates->strs[j]);
				if(strcasecmp(alternates->strs[j],
					toemails.emaillist[i]) == 0)
					intocc = 1;
			}
		}
		for(i = 0; i < ccemails.emailcount; i++) {
			log_error(LOG_ARGS, "Found Cc: %s",
				ccemails.emaillist[i]);
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
		if ((strcasecmp(listaddr, posteraddr) == 0) ||
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
		txt = open_text(listdir, "deny", "post",
				"notintocc", NULL, "notintocc");
		MY_ASSERT(txt);
		register_unformatted(txt, "subject", subject);
		register_unformatted(txt, "posteraddr", posteraddr);
		register_originalmail(txt, donemailname);
		queuefilename = prepstdreply(txt, listdir,
				"$listowner$", posteraddr, NULL);
		MY_ASSERT(queuefilename)
		close_text(txt);
		myfree(listdelim);
		myfree(listname);
		myfree(listfqdn);
		unlink(donemailname);
		myfree(donemailname);
		execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-L", listdir,
				"-T", posteraddr,
				"-F", fromaddr,
				"-m", queuefilename, (char *)NULL);

		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
		exit(EXIT_FAILURE);
	}

	subonlypost = statctrl(listdir, "subonlypost");
	if(subonlypost) {
		/* Don't send a mail about denial to the list, but silently
		 * discard and exit. */
		if (strcasecmp(listaddr, posteraddr) == 0) {
			log_error(LOG_ARGS, "Discarding %s because"
					" subonlypost was set and From: was"
					" the list address",
					mailfile);
			myfree(listaddr);
			unlink(donemailname);
			myfree(donemailname);
			exit(EXIT_SUCCESS);
		}
		if(is_subbed(listdir, posteraddr) != 0) {
			modnonsubposts = statctrl(listdir,
					"modnonsubposts");
			if(modnonsubposts) {
				moderated = 1;
				modreason = MODNONSUBPOSTS;
				goto startaccess;
			}

			nosubonlydenymails = statctrl(listdir,
					"nosubonlydenymails");

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
			fromaddr = concatstr(4, listname, listdelim,
					"bounces-help@", listfqdn);
			txt = open_text(listdir, "deny", "post",
					"subonlypost", NULL, "subonlypost");
			MY_ASSERT(txt);
			register_unformatted(txt, "subject", subject);
			register_unformatted(txt, "posteraddr", posteraddr);
			register_originalmail(txt, donemailname);
			queuefilename = prepstdreply(txt, listdir,
					"$listowner$", posteraddr, NULL);
			MY_ASSERT(queuefilename)
			close_text(txt);
			myfree(listaddr);
			myfree(listdelim);
			myfree(listname);
			myfree(listfqdn);
			unlink(donemailname);
			myfree(donemailname);
			execlp(mlmmjsend, mlmmjsend,
					"-l", "1",
					"-T", posteraddr,
					"-F", fromaddr,
					"-m", queuefilename, (char *)NULL);

			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
			exit(EXIT_FAILURE);
		}
	}

startaccess:

	if(!moderated) {
		if(statctrl(listdir, "moderated")) {
			moderated = 1;
			modreason = MODERATED;
		}
	}

	noaccessdenymails = statctrl(listdir, "noaccessdenymails");

	access_rules = ctrlvalues(listdir, "access");
	if (access_rules) {
		enum action accret;
		/* Don't send a mail about denial to the list, but silently
		 * discard and exit. Also do this in case it's turned off */
		accret = do_access(access_rules, &allheaders,
					posteraddr, listdir);
		if (accret == DENY) {
			if ((strcasecmp(listaddr, posteraddr) == 0) ||
					noaccessdenymails) {
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
			txt = open_text(listdir, "deny", "post",
					"access", NULL, "access");
			MY_ASSERT(txt);
			register_unformatted(txt, "subject", subject);
			register_unformatted(txt, "posteraddr", posteraddr);
			register_originalmail(txt, donemailname);
			queuefilename = prepstdreply(txt, listdir,
					"$listowner$", posteraddr, NULL);
			MY_ASSERT(queuefilename)
			close_text(txt);
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
					"-T", posteraddr,
					"-F", fromaddr,
					"-m", queuefilename, (char *)NULL);

			log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsend);
			exit(EXIT_FAILURE);
		} else if (accret == MODERATE) {
			moderated = 1;
			modreason = ACCESS;
		} else if (accret == DISCARD) {
	                discardname = concatstr(3, listdir,
                                "/queue/discarded/", randomstr);
			myfree(randomstr);
                	if(rename(donemailname, discardname) < 0) {
				log_error(LOG_ARGS, "could not rename(%s,%s)",
					    donemailname, discardname);
				myfree(donemailname);
				myfree(discardname);
				exit(EXIT_FAILURE);
			}
			myfree(donemailname);
			myfree(discardname);
                	exit(EXIT_SUCCESS);
		} else if (accret == SEND) {
			moderated = 0;
		} else if (accret == ALLOW) {
			/* continue processing as normal */
		}
	}

	notmetoo = statctrl(listdir, "notmetoo");

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
		if (notmetoo) {
			omitfilename = concatstr(2, mqueuename, ".omit");
			omitfd = open(omitfilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
			if (omitfd < 0) {
				log_error(LOG_ARGS, "could not open %s",
					    	    omitfilename);
				myfree(mqueuename);
				myfree(omitfilename);
				exit(EXIT_FAILURE);
			}
			myfree(omitfilename);
			if(writen(omitfd, posteraddr, strlen(posteraddr)) < 0) {
				log_error(LOG_ARGS,
						"could not write omit file");
				myfree(mqueuename);
				exit(EXIT_FAILURE);
			}
			fsync(omitfd);
			close(omitfd);
		}
		newmoderated(listdir, mqueuename,
				mlmmjsend, efrom, subject, posteraddr,
				modreason);
		return EXIT_SUCCESS;
	}

	myfree(randomstr);

	if(noprocess) {
		myfree(donemailname);
		/* XXX: toemails and ccemails etc. have to be myfree() */
		exit(EXIT_SUCCESS);
	}

	if (notmetoo)
		execlp(mlmmjsend, mlmmjsend,
				"-L", listdir,
				"-o", posteraddr,
				"-m", donemailname, (char *)NULL);
	else
		execlp(mlmmjsend, mlmmjsend,
				"-L", listdir,
				"-m", donemailname, (char *)NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);

	return EXIT_FAILURE;
}
