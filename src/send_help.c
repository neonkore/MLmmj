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
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mlmmj.h"
#include "send_help.h"
#include "strgen.h"
#include "find_email_adr.h"
#include "getlistaddr.h"
#include "log_error.h"
#include "chomp.h"
#include "wrappers.h"
#include "mygetline.h"
#include "prepstdreply.h"

void send_help(const char *listdir, const char *emailaddr,
	       const char *mlmmjsend)
{
	char *queuefilename, *listaddr, *listname, *listfqdn, *fromaddr;
	char *fromstr, *subject;
	char *maildata[] = { "*UNSUBADDR*", NULL, "*SUBADDR*", NULL,
			     "*HLPADDR*", NULL };

        listaddr = getlistaddr(listdir);
	chomp(listaddr);

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

	fromaddr = concatstr(3, listname, "+bounces-help@", listfqdn);

        maildata[1] = concatstr(3, listname, "+unsubscribe@", listfqdn);
	maildata[3] = concatstr(3, listname, "+subscribe@", listfqdn);
	maildata[5] = concatstr(3, listname, "+help@", listfqdn);
	fromstr = concatstr(3, listname, "+owner@", listfqdn);
	subject = concatstr(2, "Help for ", listaddr);

	queuefilename = prepstdreply(listdir, "listhelp", fromstr, emailaddr,
				NULL, subject, 3, maildata);
	if(queuefilename == NULL) {
		log_error(LOG_ARGS, "Could not prepare help mail");
		exit(EXIT_FAILURE);
	}
	
	free(fromstr);
	free(listaddr);
	free(listname);
	free(listfqdn);
	free(maildata[1]);
	free(maildata[3]);
	free(maildata[5]);
	free(subject);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", emailaddr,
				"-F", fromaddr,
				"-m", queuefilename, 0);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}
