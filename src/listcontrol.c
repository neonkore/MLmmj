/* Copyright (C) 2003 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "mlmmj.h"
#include "listcontrol.h"
#include "find_email_adr.h"
#include "strgen.h"
#include "send_help.h"
#include "log_error.h"
#include "statctrl.h"
#include "mygetline.h"
#include "chomp.h"
#include "memory.h"

enum ctrl_e {
	CTRL_SUBSCRIBE_DIGEST,
	CTRL_SUBSCRIBE_NOMAIL,
	CTRL_SUBSCRIBE,
	CTRL_CONFSUB_DIGEST,
	CTRL_CONFSUB_NOMAIL,
	CTRL_CONFSUB,
	CTRL_UNSUBSCRIBE_DIGEST,
	CTRL_UNSUBSCRIBE_NOMAIL,
	CTRL_UNSUBSCRIBE,
	CTRL_CONFUNSUB_DIGEST,
	CTRL_CONFUNSUB_NOMAIL,
	CTRL_CONFUNSUB,
	CTRL_BOUNCES,
	CTRL_MODERATE,
	CTRL_HELP,
	CTRL_GET,
	CTRL_END  /* end marker, must be last */
};

struct ctrl_command {
	char *command;
	unsigned int accepts_parameter;
};

/* Must match the enum. CAREFUL when using commands that are substrings
 * of other commands. In that case the longest one have to be listed
 * first to match correctly. */
static struct ctrl_command ctrl_commands[] = {
	{ "subscribe-digest",   0 },
	{ "subscribe-nomail",   0 },
	{ "subscribe",          0 },
	{ "confsub-digest",     1 },
	{ "confsub-nomail",     1 },
	{ "confsub",            1 },
	{ "unsubscribe-digest", 0 },
	{ "unsubscribe-nomail", 0 },
	{ "unsubscribe",        0 },
	{ "confunsub-digest",   1 },
	{ "confunsub-nomail",   1 },
	{ "confunsub",          1 },
	{ "bounces",            1 },
	{ "moderate",           1 },
	{ "help",               0 },
	{ "get",                1 }
};


int listcontrol(struct email_container *fromemails, const char *listdir,
		const char *controladdr, const char *mlmmjsub,
		const char *mlmmjunsub, const char *mlmmjsend,
		const char *mlmmjbounce, const char *mailname)
{
	char *atsign, *recipdelimsign, *bouncenr, *tmpstr;
	char *controlstr, *param, *conffilename, *moderatefilename;
	char *c, *archivefilename;
	const char *subswitch;
	size_t len;
	struct stat stbuf;
	int closedlist, nosubconfirm, tmpfd, noget;
	size_t cmdlen;
	unsigned int ctrl;
	
	/* A closed list doesn't allow subscribtion and unsubscription */
	closedlist = statctrl(listdir, "closedlist");

	nosubconfirm = statctrl(listdir, "nosubconfirm");
	if(nosubconfirm)
		subswitch = "-c";
	else
		subswitch = "-C";
	
	recipdelimsign = index(controladdr, RECIPDELIM);
	MY_ASSERT(recipdelimsign);
	atsign = index(controladdr, '@');
	MY_ASSERT(atsign);
	len = atsign - recipdelimsign;

	controlstr = mymalloc(len);
	MY_ASSERT(controlstr);
	snprintf(controlstr, len, "%s", recipdelimsign + 1);

#if 0
	log_error(LOG_ARGS, "controlstr = [%s]\n", controlstr);
	log_error(LOG_ARGS, "fromemails->emaillist[0] = [%s]\n",
			fromemails->emaillist[0]);
#endif
	for (ctrl=0; ctrl<CTRL_END; ctrl++) {
		cmdlen = strlen(ctrl_commands[ctrl].command);
		if (strncmp(controlstr, ctrl_commands[ctrl].command, cmdlen)
				== 0) {

			if (ctrl_commands[ctrl].accepts_parameter &&
					(controlstr[cmdlen] == '-')) {
				param = mystrdup(controlstr + cmdlen + 1);
				MY_ASSERT(param);
				if (strchr(param, '/')) {
					errno = 0;
					log_error(LOG_ARGS, "Slash (/) in"
						" list control request,"
						" discarding mail");
					exit(EXIT_SUCCESS);
				}
			} else if (!ctrl_commands[ctrl].accepts_parameter &&
					(controlstr[cmdlen] == '\0')) {
				param = NULL;
			} else {
				/* malformed request, ignore and clean up */
				unlink(mailname);
				exit(EXIT_SUCCESS);
			}

			myfree(controlstr);
			break;

		}
	}

	switch (ctrl) {

	/* listname+subscribe-digest@domain.tld */
	case CTRL_SUBSCRIBE_DIGEST:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		if (!strchr(fromemails->emaillist[0], '@'))
			/* Not a valid From: address, silently ignore */
			exit(EXIT_SUCCESS);
		execlp(mlmmjsub, mlmmjsub,
				"-L", listdir,
				"-a", fromemails->emaillist[0],
				"-d",
				subswitch, NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+subscribe-nomail@domain.tld */
	case CTRL_SUBSCRIBE_NOMAIL:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		if (!strchr(fromemails->emaillist[0], '@'))
			/* Not a valid From: address, silently ignore */
			exit(EXIT_SUCCESS);
		execlp(mlmmjsub, mlmmjsub,
				"-L", listdir,
				"-a", fromemails->emaillist[0],
				"-n",
				subswitch, NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+subscribe@domain.tld */
	case CTRL_SUBSCRIBE:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		if (!strchr(fromemails->emaillist[0], '@'))
			/* Not a valid From: address, silently ignore */
			exit(EXIT_SUCCESS);
		execlp(mlmmjsub, mlmmjsub,
				"-L", listdir,
				"-a", fromemails->emaillist[0],
				subswitch, NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+subconf-digest-COOKIE@domain.tld */
	case CTRL_CONFSUB_DIGEST:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/subconf/", param);
		myfree(param);
		if((tmpfd = open(conffilename, O_RDONLY)) < 0) {
			/* invalid COOKIE, silently ignore */
			exit(EXIT_SUCCESS);
		}
		tmpstr = mygetline(tmpfd);
		chomp(tmpstr);
		close(tmpfd);
		unlink(conffilename);
		execlp(mlmmjsub, mlmmjsub,
				"-L", listdir,
				"-a", tmpstr,
				"-d",
				"-c", NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+subconf-nomail-COOKIE@domain.tld */
	case CTRL_CONFSUB_NOMAIL:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/subconf/", param);
		myfree(param);
		if((tmpfd = open(conffilename, O_RDONLY)) < 0) {
			/* invalid COOKIE, silently ignore */
			exit(EXIT_SUCCESS);
		}
		tmpstr = mygetline(tmpfd);
		chomp(tmpstr);
		close(tmpfd);
		unlink(conffilename);
		execlp(mlmmjsub, mlmmjsub,
				"-L", listdir,
				"-a", tmpstr,
				"-n",
				"-c", NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+subconf-COOKIE@domain.tld */
	case CTRL_CONFSUB:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/subconf/", param);
		myfree(param);
		if((tmpfd = open(conffilename, O_RDONLY)) < 0) {
			/* invalid COOKIE, silently ignore */
			exit(EXIT_SUCCESS);
		}
		tmpstr = mygetline(tmpfd);
		chomp(tmpstr);
		close(tmpfd);
		unlink(conffilename);
		execlp(mlmmjsub, mlmmjsub,
				"-L", listdir,
				"-a", tmpstr,
				"-c", NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+unsubscribe-digest@domain.tld */
	case CTRL_UNSUBSCRIBE_DIGEST:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		if (!strchr(fromemails->emaillist[0], '@'))
			/* Not a valid From: address, silently ignore */
			exit(EXIT_SUCCESS);
		execlp(mlmmjunsub, mlmmjunsub,
				"-L", listdir,
				"-a", fromemails->emaillist[0],
				"-d",
				subswitch, NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjunsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+unsubscribe-nomail@domain.tld */
	case CTRL_UNSUBSCRIBE_NOMAIL:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		if (!strchr(fromemails->emaillist[0], '@'))
			/* Not a valid From: address, silently ignore */
			exit(EXIT_SUCCESS);
		execlp(mlmmjunsub, mlmmjunsub,
				"-L", listdir,
				"-a", fromemails->emaillist[0],
				"-n",
				subswitch, NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjunsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+unsubscribe@domain.tld */
	case CTRL_UNSUBSCRIBE:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		if (!strchr(fromemails->emaillist[0], '@'))
			/* Not a valid From: address, silently ignore */
			exit(EXIT_SUCCESS);
		execlp(mlmmjunsub, mlmmjunsub,
				"-L", listdir,
				"-a", fromemails->emaillist[0],
				subswitch, NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjunsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+unsubconf-digest-COOKIE@domain.tld */
	case CTRL_CONFUNSUB_DIGEST:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/unsubconf/", param);
		myfree(param);
		if((tmpfd = open(conffilename, O_RDONLY)) < 0) {
			/* invalid COOKIE, silently ignore */
			exit(EXIT_SUCCESS);
		}
		tmpstr = mygetline(tmpfd);
		close(tmpfd);
		chomp(tmpstr);
		unlink(conffilename);
		execlp(mlmmjunsub, mlmmjunsub,
				"-L", listdir,
				"-a", tmpstr,
				"-d",
				"-c", NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjunsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+unsubconf-nomail-COOKIE@domain.tld */
	case CTRL_CONFUNSUB_NOMAIL:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/unsubconf/", param);
		myfree(param);
		if((tmpfd = open(conffilename, O_RDONLY)) < 0) {
			/* invalid COOKIE, silently ignore */
			exit(EXIT_SUCCESS);
		}
		tmpstr = mygetline(tmpfd);
		close(tmpfd);
		chomp(tmpstr);
		unlink(conffilename);
		execlp(mlmmjunsub, mlmmjunsub,
				"-L", listdir,
				"-a", tmpstr,
				"-n",
				"-c", NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjunsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+unsubconf-COOKIE@domain.tld */
	case CTRL_CONFUNSUB:
		unlink(mailname);
		if (closedlist)
			exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/unsubconf/", param);
		myfree(param);
		if((tmpfd = open(conffilename, O_RDONLY)) < 0) {
			/* invalid COOKIE, silently ignore */
			exit(EXIT_SUCCESS);
		}
		tmpstr = mygetline(tmpfd);
		close(tmpfd);
		chomp(tmpstr);
		unlink(conffilename);
		execlp(mlmmjunsub, mlmmjunsub,
				"-L", listdir,
				"-a", tmpstr,
				"-c", NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
				mlmmjunsub);
		exit(EXIT_FAILURE);
		break;

	/* listname+bounces-INDEX-user=example.tld@domain.tld */
	case CTRL_BOUNCES:
		bouncenr = param;
		c = strchr(param, '-');
		if (!c) { /* malformed bounce, ignore and clean up */
	 		unlink(mailname);
			exit(EXIT_SUCCESS);
		}
		*c++ = '\0';
		execlp(mlmmjbounce, mlmmjbounce,
				"-L", listdir,
				"-a", c,
				"-m", mailname,
				"-n", bouncenr, NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjbounce);
		exit(EXIT_FAILURE);
		break;

	/* listname+moderate-COOKIE@domain.tld */
	case CTRL_MODERATE:
		/* TODO Add accept/reject parameter to moderate */
		unlink(mailname);
		moderatefilename = concatstr(3, listdir, "/moderation/", param);
		myfree(param);
		if(stat(moderatefilename, &stbuf) < 0) {
			myfree(moderatefilename);
			exit(EXIT_SUCCESS); /* just exit, no mail to moderate */
		}
		execlp(mlmmjsend, mlmmjsend,
				"-L", listdir,
				"-m", moderatefilename, NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsend);
		exit(EXIT_FAILURE);
		break;

	/* listname+help@domain.tld */
	case CTRL_HELP:
		unlink(mailname);
		if(strchr(fromemails->emaillist[0], '@'))
			send_help(listdir, fromemails->emaillist[0],
				  mlmmjsend);
		break;

	/* listname+get-INDEX@domain.tld */
	case CTRL_GET:
		unlink(mailname);
		noget = statctrl(listdir, "noget");
		if(noget)
			exit(EXIT_SUCCESS);
		/* sanity check--is it all digits? */
		for(c = param; *c != '\0'; c++) {
			if(!isdigit((int)*c))
				exit(EXIT_SUCCESS);
		}
		archivefilename = concatstr(3, listdir, "/archive/",
						param);
		if(stat(archivefilename, &stbuf) < 0)
			exit(EXIT_SUCCESS);
		execlp(mlmmjsend, mlmmjsend,
				"-T", fromemails->emaillist[0],
				"-L", listdir,
				"-l", "6",
				"-m", archivefilename,
				"-a", "-D", NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsend);
		exit(EXIT_FAILURE);
		break;
	}

	unlink(mailname);

	return 0;
}
