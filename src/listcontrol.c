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

#include "mlmmj.h"
#include "header_token.h"
#include "listcontrol.h"
#include "find_email_adr.h"
#include "strgen.h"
#include "send_help.h"
#include "log_error.h"

int listcontrol(const char *mailfilename, const char *listdir,
		const char *controladdr, const char *mlmmjsub,
		const char *mlmmjunsub, const char *mlmmjsend,
		const char *mlmmjbounce)
{
	char tmpstr[READ_BUFSIZE];
	char *atsign, *recipdelimsign, *tokenvalue, *confstr, *bouncenr;
	char *controlstr, *conffilename;
	FILE *mailfile, *tempfile;
	struct email_container fromemails;
	size_t len;
	
	if((mailfile = fopen(mailfilename, "r")) == NULL) {
		log_error(LOG_ARGS, "listcontrol, could not open mail");
		exit(EXIT_FAILURE);
	}
	recipdelimsign = index(controladdr, RECIPDELIM);
	atsign = index(controladdr, '@');
	len = atsign - recipdelimsign;

	controlstr = malloc(len);
	snprintf(controlstr, len, "%s", recipdelimsign + 1);

	tokenvalue = find_header_file(mailfile, tmpstr, "From:");
	fclose(mailfile);
	unlink(mailfilename);

	find_email_adr(tmpstr, &fromemails);

#if 0
	log_error(LOG_ARGS, "controlstr = [%s]\n", controlstr);
#endif

	if(strncasecmp(controlstr, "subscribe", 9) == 0) {
		free(controlstr);
		if(index(fromemails.emaillist[0], '@')) {
			execlp(mlmmjsub, mlmmjsub,
					"-L", listdir,
					"-a", fromemails.emaillist[0],
					"-C", 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsub);
			exit(EXIT_FAILURE);
		} else /* Not a valid From: address, so we silently ignore */
			exit(EXIT_SUCCESS);
	} else if(strncasecmp(controlstr, "confsub-", 8) == 0) {
		len -= 7;
		confstr = malloc(len);
		snprintf(confstr, len, "%s", controlstr + 8);
		free(controlstr);
		conffilename = concatstr(3, listdir, "/subconf/", confstr);
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
	} else if(strncasecmp(controlstr, "unsubscribe", 11) == 0) {
		free(controlstr);
		if(index(fromemails.emaillist[0], '@')) {
			execlp(mlmmjunsub, mlmmjunsub,
					"-L", listdir,
					"-a", fromemails.emaillist[0],
					"-C", 0);
			log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjunsub);
			exit(EXIT_FAILURE);
		} else /* Not a valid From: address, so we silently ignore */
			exit(EXIT_SUCCESS);
	} else if(strncasecmp(controlstr, "confunsub-", 10) == 0) {
		len -= 9;
		confstr = malloc(len);
		snprintf(confstr, len, "%s", controlstr + 10);
		free(controlstr);
		conffilename = concatstr(3, listdir, "/unsubconf/", confstr);
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
				log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjunsub);
				exit(EXIT_FAILURE);
			} else {
				exit(EXIT_SUCCESS);
			}
		} else /* Not a confirm so silently ignore */
			exit(EXIT_SUCCESS);
	} else if(strncasecmp(controlstr, "bounces-", 8) == 0) {
		controlstr += 8;
		bouncenr = strrchr(controlstr, '-');
		if (!bouncenr) exit(EXIT_SUCCESS);  /* malformed bounce, ignore */
		*bouncenr++ = '\0';
#if 0
		log_error(LOG_ARGS, "bounce, bounce, bounce email=[%s] nr=[%s]", controlstr, bouncenr);
#endif
		execlp(mlmmjbounce, mlmmjbounce,
				"-L", listdir,
				"-a", controlstr,
				"-n", bouncenr, 0);
		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjbounce);
	} else if(strncasecmp(controlstr, "help", 4) == 0) {
		printf("Help wanted!\n");
		free(controlstr);
		if(index(fromemails.emaillist[0], '@'))
			send_help(listdir, fromemails.emaillist[0],
				  mlmmjsend);
	}
	return 0;
}
