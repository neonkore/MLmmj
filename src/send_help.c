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
#include "send_help.h"
#include "strgen.h"
#include "find_email_adr.h"
#include "getlistaddr.h"
#include "log_error.h"

void send_help(const char *listdir, const char *emailaddr,
	       const char *mlmmjsend)
{
	FILE *helpfile, *queuefile;
	char buf[READ_BUFSIZE];
	char *listaddr, *bufres, *helpaddr, *fromaddr, *fromstr, *tostr;
	char *subjectstr, *helpfilename, *queuefilename, *listname;
	char *randomstr, *listfqdn;
	size_t len;

        listaddr = getlistaddr(listdir);

	helpfilename = concatstr(2, listdir, "/text/listhelp");

	if((helpfile = fopen(helpfilename, "r")) == NULL) {
		log_error(LOG_ARGS, "Could not open text/help");
		free(helpfilename);
		exit(EXIT_FAILURE);
	}

	free(helpfilename);

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	randomstr = random_str();

	queuefilename = concatstr(3, listdir, "/queue/", randomstr);
	printf("%s\n", queuefilename);
	
	if((queuefile = fopen(queuefilename, "w")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		free(queuefilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	free(randomstr);

	len = strlen(listname) + strlen(listfqdn) + strlen("+help@") + 1;
	helpaddr = malloc(len);
	snprintf(helpaddr, len, "%s+help@%s", listname, listfqdn);

	len += strlen("+bounces");
	fromaddr = malloc(len);
	snprintf(fromaddr, len, "%s+bounces-help@%s", listname, listfqdn);

	fromstr = headerstr("From: ", helpaddr);
	fputs(fromstr, queuefile);
	free(helpaddr);

	tostr = headerstr("To: ", emailaddr);
	fputs(tostr, queuefile);

	subjectstr = headerstr("Subject: Help for ", listaddr);
	fputs(subjectstr, queuefile);
	fputc('\n', queuefile);

	while((bufres = fgets(buf, READ_BUFSIZE, helpfile))) {
		if(strncmp(buf, "*UNSUBADDR*", 11) == 0) {
			fputs(listname, queuefile);
			fputs("+unsubscribe@", queuefile);
			fputs(listfqdn, queuefile);
		}
		else if(strncmp(buf, "*SUBADDR*", 9) == 0) {
			fputs(listname, queuefile);
			fputs("+subscribe@", queuefile);
			fputs(listfqdn, queuefile);
		}
		else if(strncmp(buf, "*HLPADDR*", 9) == 0) {
			fputs(listname, queuefile);
			fputs("+help@", queuefile);
			fputs(listfqdn, queuefile);
		} else
			fputs(buf, queuefile);
	}
	
	free(tostr);
	free(subjectstr);
	free(listname);
	free(listfqdn);
	fclose(helpfile);
	fclose(queuefile);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", emailaddr,
				"-F", fromaddr,
				"-m", queuefilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}
