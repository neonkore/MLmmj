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

void send_help(const char *listdir, const char *emailaddr,
	       const char *mlmmjsend)
{
	int helpfd, queuefd;
	char *listaddr, *buf, *fromaddr;
	char *helpfilename, *queuefilename, *listname;
	char *randomstr, *listfqdn, *s1;

        listaddr = getlistaddr(listdir);
	chomp(listaddr);

	helpfilename = concatstr(2, listdir, "/text/listhelp");

	if((helpfd = open(helpfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open text/help");
		free(helpfilename);
		exit(EXIT_FAILURE);
	}

	free(helpfilename);

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	randomstr = random_str();

	queuefilename = concatstr(3, listdir, "/queue/", randomstr);
	
	queuefd = open(queuefilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	if(queuefd < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		free(queuefilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	free(randomstr);

	fromaddr = concatstr(3, listname, "+bounces-help@", listfqdn);

	s1 = concatstr(11, "From: ", listname, "+owner@", listfqdn, "\n"
			"To: ", emailaddr, "\n", "Subject: Help for ",
			listaddr, "\n\n");

	if(writen(queuefd, s1, strlen(s1)) < 0) {
		log_error(LOG_ARGS, "Could not write help mail");
		exit(EXIT_FAILURE);
	}

	free(s1);

	while((buf = mygetline(helpfd)) != NULL) {
		if(strncmp(buf, "*UNSUBADDR*", 11) == 0) {
			s1 = concatstr(3, listname, "+unsubscribe@", listfqdn);
			if(writen(queuefd, s1, strlen(s1)) < 0) {
				log_error(LOG_ARGS,
						"Could not write help mail");
				exit(EXIT_FAILURE);
			}
			free(s1);
		} else if(strncmp(buf, "*SUBADDR*", 9) == 0) {
			s1 = concatstr(3, listname, "+subscribe@", listfqdn);
			if(writen(queuefd, s1, strlen(s1)) < 0) {
				log_error(LOG_ARGS,
						"Could not write help mail");
				exit(EXIT_FAILURE);
			}
			free(s1);
		} else if(strncmp(buf, "*HLPADDR*", 9) == 0) {
			s1 = concatstr(3, listname, "+help@", listfqdn);
			if(writen(queuefd, s1, strlen(s1)) < 0) {
				log_error(LOG_ARGS,
						"Could not write help mail");
				exit(EXIT_FAILURE);
			}
			free(s1);
		} else if(writen(queuefd, buf, strlen(buf)) < 0) {
				log_error(LOG_ARGS,
						"Could not write help mail");
				exit(EXIT_FAILURE);
		}
		free(buf);
	}
	
	free(listname);
	free(listfqdn);
	close(helpfd);
	close(queuefd);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", emailaddr,
				"-F", fromaddr,
				"-m", queuefilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}
