/* Copyright (C) 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mlmmj.h"
#include "header_token.h"
#include "listcontrol.h"
#include "find_email_adr.h"
#include "strgen.h"
#include "send_help.h"
#include "log_error.h"
#include "statctrl.h"

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


int listcontrol(const char *mailfilename, const char *listdir,
		const char *controladdr, const char *mlmmjsub,
		const char *mlmmjunsub, const char *mlmmjsend,
		const char *mlmmjbounce)
{
	char tmpstr[READ_BUFSIZE];
	char *atsign, *recipdelimsign, *tokenvalue, *bouncenr;
	char *controlstr, *param, *conffilename, *moderatefilename;
	FILE *mailfile, *tempfile;
	struct email_container fromemails;
	size_t len;
	struct stat stbuf;
	int closedlist;
	size_t cmdlen;
	unsigned int ctrl;
	
	if((mailfile = fopen(mailfilename, "r")) == NULL) {
		log_error(LOG_ARGS, "listcontrol, could not open mail");
		exit(EXIT_FAILURE);
	}

	/* A closed list doesn't allow subscribtion and unsubscription */
	closedlist = statctrl(listdir, "closedlist");

	recipdelimsign = index(controladdr, RECIPDELIM);
	MY_ASSERT(recipdelimsign);
	atsign = index(controladdr, '@');
	MY_ASSERT(atsign);
	len = atsign - recipdelimsign;

	controlstr = malloc(len);
	MY_ASSERT(controlstr);
	snprintf(controlstr, len, "%s", recipdelimsign + 1);

	tokenvalue = find_header_file(mailfile, tmpstr, "From:");
	fclose(mailfile);
	unlink(mailfilename);

	find_email_adr(tmpstr, &fromemails);

#if 0
	log_error(LOG_ARGS, "controlstr = [%s]\n", controlstr);
#endif
	for (ctrl=0; ctrl<CTRL_END; ctrl++) {
		cmdlen = strlen(ctrl_commands[ctrl].command);
		if (strncmp(controlstr, ctrl_commands[ctrl].command, cmdlen)
				== 0) {

			if (ctrl_commands[ctrl].accepts_parameter &&
					(controlstr[cmdlen] == '-')) {
				param = strdup(controlstr + cmdlen + 1);
				MY_ASSERT(param);
				free(controlstr);
				break;
			} else if (!ctrl_commands[ctrl].accepts_parameter &&
					(controlstr[cmdlen] == '\0')) {
				param = NULL;
				free(controlstr);
				break;
			} else {
				log_error(LOG_ARGS, "Received a malformed"
					" list control request");
				free(controlstr);
				return -1;
			}

		}
	}

	switch (ctrl) {

	case CTRL_SUBSCRIBE:
		if (closedlist) exit(EXIT_SUCCESS);
		if(index(fromemails.emaillist[0], '@')) {
			execlp(mlmmjsub, mlmmjsub,
					"-L", listdir,
					"-a", fromemails.emaillist[0],
					"-C", 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsub);
			exit(EXIT_FAILURE);
		} else /* Not a valid From: address, so we silently ignore */
			exit(EXIT_SUCCESS);
		break;

	case CTRL_CONFSUB:
		if (closedlist) exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/subconf/", param);
		free(param);
		if((tempfile = fopen(conffilename, "r"))) {
			fgets(tmpstr, READ_BUFSIZE, tempfile);
			fclose(tempfile);
			if(strncasecmp(tmpstr, fromemails.emaillist[0],
						strlen(tmpstr)) == 0) {
				unlink(conffilename);
				execlp(mlmmjsub, mlmmjsub,
						"-L", listdir,
						"-a", tmpstr,
						"-c", 0);
				log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsub);
				exit(EXIT_FAILURE);
			} else {
				/* Not proper confirm */
				exit(EXIT_SUCCESS);
			}
		} else /* Not a confirm so silently ignore */
			exit(EXIT_SUCCESS);
		break;

	case CTRL_UNSUBSCRIBE:
		if (closedlist) exit(EXIT_SUCCESS);
		if(index(fromemails.emaillist[0], '@')) {
			execlp(mlmmjunsub, mlmmjunsub,
					"-L", listdir,
					"-a", fromemails.emaillist[0],
					"-C", 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjunsub);
			exit(EXIT_FAILURE);
		} else /* Not a valid From: address, so we silently ignore */
			exit(EXIT_SUCCESS);
		break;

	case CTRL_CONFUNSUB:
		if (closedlist) exit(EXIT_SUCCESS);
		conffilename = concatstr(3, listdir, "/unsubconf/", param);
		free(param);
		if((tempfile = fopen(conffilename, "r"))) {
			fgets(tmpstr, READ_BUFSIZE, tempfile);
			fclose(tempfile);
			if(strncasecmp(tmpstr, fromemails.emaillist[0],
						strlen(tmpstr)) == 0) {
				unlink(conffilename);
				execlp(mlmmjunsub, mlmmjunsub,
						"-L", listdir,
						"-a", tmpstr,
						"-c", 0);
				log_error(LOG_ARGS, "execlp() of '%s' failed",
						    mlmmjunsub);
				exit(EXIT_FAILURE);
			} else {
				exit(EXIT_SUCCESS);
			}
		} else /* Not a confirm so silently ignore */
			exit(EXIT_SUCCESS);
		break;

	case CTRL_BOUNCES:
		bouncenr = strrchr(param, '-');
		if (!bouncenr) exit(EXIT_SUCCESS); /* malformed bounce, ignore */
		*bouncenr++ = '\0';
		execlp(mlmmjbounce, mlmmjbounce,
				"-L", listdir,
				"-a", param,
				"-n", bouncenr, 0);
		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjbounce);
		exit(EXIT_FAILURE);
		break;

	case CTRL_MODERATE:
		/* TODO Add accept/reject parameter to moderate */
		moderatefilename = concatstr(3, listdir, "/moderation/queue/",
						       param);
		free(param);
		if(stat(moderatefilename, &stbuf) < 0) {
			free(moderatefilename);
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
		printf("Help wanted!\n");
		if(index(fromemails.emaillist[0], '@'))
			send_help(listdir, fromemails.emaillist[0],
				  mlmmjsend);
		break;

	}

	return 0;
}
