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
		const char *controladdr)
{
	char *atsign;
	char *recipdelimsign;
	char tmpstr[READ_BUFSIZE];
	char *tokenvalue;
	char *confstr;
	char *controlstr;
	char *conffilename;
	FILE *mailfile, *tempfile;
	struct email_container fromemails;
	size_t len;
	
	if((mailfile = fopen(mailfilename, "r")) == NULL) {
		log_error("listcontrol, could not open mail");
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

	printf("controlstr = [%s]\n", controlstr);

	if(strncasecmp(controlstr, "subscribe", 9) == 0) {
		free(controlstr);
		if(index(fromemails.emaillist[0], '@')) {
			execlp(BINDIR"mlmmj-sub", "mlmmj-sub",
					"-L", listdir,
					"-a", fromemails.emaillist[0],
					"-C", 0);
			log_error("execlp() of "BINDIR"mlmmj-sub failed");
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
				execlp(BINDIR"mlmmj-sub", "mlmmj-sub",
						"-L", listdir,
						"-a", tmpstr,
						"-c", 0);
				log_error("execlp() of "BINDIR"mlmmj-sub"
					" failed");
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
			execlp(BINDIR"mlmmj-unsub", "mlmmj-unsub",
					"-L", listdir,
					"-a", fromemails.emaillist[0],
					"-C", 0);
			log_error("execlp() of "BINDIR"mlmmj-unsub"
				" failed");
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
				execlp(BINDIR"mlmmj-unsub", "mlmmj-unsub",
						"-L", listdir,
						"-a", tmpstr,
						"-c", 0);
				log_error("execlp() of "
					BINDIR"mlmmj-unsub failed");
				exit(EXIT_FAILURE);
			} else {
				exit(EXIT_SUCCESS);
			}
		} else /* Not a confirm so silently ignore */
			exit(EXIT_SUCCESS);
	} else if(strncasecmp(controlstr, "help", 4) == 0) {
		printf("Help wanted!\n");
		free(controlstr);
		if(index(fromemails.emaillist[0], '@'))
			send_help(listdir, fromemails.emaillist[0]);
	}
	return 0;
}
