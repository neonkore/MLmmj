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
	CTRL_SUBSCRIBE,
	CTRL_CONFSUB,
	CTRL_UNSUBSCRIBE,
	CTRL_CONFUNSUB,
	CTRL_BOUNCES,
	CTRL_MODERATE,
	CTRL_HELP,
	CTRL_END  /* end marker, must be last */
};

/* MMJ says that function pointers are just for show off, so I will
 * switch an enum :-)  -- mortenp 20040511 */
struct ctrl_command {
	char *command;
	unsigned int accepts_parameter;
};

/* must match the enum */
static struct ctrl_command ctrl_commands[] = {
	{ "subscribe",   0 },
	{ "confsub",     1 },
	{ "unsubscribe", 0 },
	{ "confunsub",   1 },
	{ "bounces",     1 },
	{ "moderate",    1 },
	{ "help",        0 }
};


int listcontrol(struct email_container *fromemails, const char *listdir,
		const char *controladdr, const char *mlmmjsub,
		const char *mlmmjunsub, const char *mlmmjsend,
		const char *mlmmjbounce, const char *mailname)
{
	char *atsign, *recipdelimsign, *bouncenr, *tmpstr;
	char *controlstr, *param, *conffilename, *moderatefilename;
	size_t len;
	struct stat stbuf;
	int closedlist, tmpfd;
	size_t cmdlen;
	unsigned int ctrl;
	
	/* A closed list doesn't allow subscribtion and unsubscription */
	closedlist = statctrl(listdir, "closedlist");

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
				myfree(controlstr);
				break;
			} else if (!ctrl_commands[ctrl].accepts_parameter &&
					(controlstr[cmdlen] == '\0')) {
				param = NULL;
				myfree(controlstr);
				break;
			} else {
				log_error(LOG_ARGS, "Received a malformed"
					" list control request");
				myfree(controlstr);
				return -1;
			}

		}
	}

	switch (ctrl) {

	case CTRL_SUBSCRIBE:
		unlink(mailname);
		if (closedlist) exit(EXIT_SUCCESS);
		if(strchr(fromemails->emaillist[0], '@')) {
			execlp(mlmmjsub, mlmmjsub,
					"-L", listdir,
					"-a", fromemails->emaillist[0],
					"-C", 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsub);
			exit(EXIT_FAILURE);
		} else /* Not a valid From: address, so we silently ignore */
			exit(EXIT_SUCCESS);
		break;

	case CTRL_CONFSUB:
		unlink(mailname);
		if (closedlist) exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/subconf/", param);
		myfree(param);
		if((tmpfd = open(conffilename, O_RDONLY)) >= 0) {
			tmpstr = mygetline(tmpfd);
			chomp(tmpstr);
			close(tmpfd);
			unlink(conffilename);
			execlp(mlmmjsub, mlmmjsub,
					"-L", listdir,
					"-a", tmpstr,
					"-c", 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjsub);
			exit(EXIT_FAILURE);
		} else /* Not a confirm so silently ignore */
			exit(EXIT_SUCCESS);
		break;

	case CTRL_UNSUBSCRIBE:
		unlink(mailname);
		if (closedlist) exit(EXIT_SUCCESS);
		if(strchr(fromemails->emaillist[0], '@')) {
			execlp(mlmmjunsub, mlmmjunsub,
					"-L", listdir,
					"-a", fromemails->emaillist[0],
					"-C", 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjunsub);
			exit(EXIT_FAILURE);
		} else /* Not a valid From: address, so we silently ignore */
			exit(EXIT_SUCCESS);
		break;

	case CTRL_CONFUNSUB:
		unlink(mailname);
		if (closedlist) exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/unsubconf/", param);
		myfree(param);
		if((tmpfd = open(conffilename, O_RDONLY)) >= 0) {
			tmpstr = mygetline(tmpfd);
			close(tmpfd);
			chomp(tmpstr);
			unlink(conffilename);
			execlp(mlmmjunsub, mlmmjunsub,
					"-L", listdir,
					"-a", tmpstr,
					"-c", 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed",
					mlmmjunsub);
			exit(EXIT_FAILURE);
		} else /* Not a confirm so silently ignore */
			exit(EXIT_SUCCESS);
		break;

	case CTRL_BOUNCES:
		bouncenr = strrchr(param, '-');
		if (!bouncenr) { /* malformed bounce, ignore and clean up */
			unlink(mailname);
			exit(EXIT_SUCCESS);
		}
		*bouncenr++ = '\0';
		execlp(mlmmjbounce, mlmmjbounce,
				"-L", listdir,
				"-a", param,
				"-m", mailname,
				"-n", bouncenr, 0);
		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjbounce);
		exit(EXIT_FAILURE);
		break;

	case CTRL_MODERATE:
		/* TODO Add accept/reject parameter to moderate */
		unlink(mailname);
		moderatefilename = concatstr(3, listdir, "/moderation/", param);
		myfree(param);
		if(stat(moderatefilename, &stbuf) < 0) {
			myfree(moderatefilename);
			exit(EXIT_SUCCESS); /* just exit, no mail to moderate */
		} else {
			execlp(mlmmjsend, mlmmjsend,
					"-L", listdir,
					"-m", moderatefilename, 0);
			log_error(LOG_ARGS, "execlp() of %s failed", mlmmjsend);
			exit(EXIT_FAILURE);
		}
		break;

	case CTRL_HELP:
		unlink(mailname);
		if(strchr(fromemails->emaillist[0], '@'))
			send_help(listdir, fromemails->emaillist[0],
				  mlmmjsend);
		break;

	}

	unlink(mailname);

	return 0;
}
