/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#include "mlmmj.h"
#include "mlmmj-sub.h"
#include "mylocking.h"
#include "wrappers.h"
#include "getlistaddr.h"
#include "strgen.h"
#include "subscriberfuncs.h"
#include "log_error.h"

void confirm_sub(const char *listdir, const char *listaddr,
		const char *subaddr, const char *mlmmjsend)
{
	FILE *subtextfile, *queuefile;
	char buf[READ_BUFSIZE];
	char *bufres, *subtextfilename, *randomstr, *queuefilename;
	char *fromstr, *tostr, *subjectstr, *fromaddr, *helpaddr;
	char *listname, *listfqdn;

	subtextfilename = concatstr(2, listdir, "/text/sub-ok");

	if((subtextfile = fopen(subtextfilename, "r")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", subtextfilename);
		free(subtextfilename);
		exit(EXIT_FAILURE);
	}
	free(subtextfilename);

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

	helpaddr = concatstr(3, listname, "+help@", listfqdn);

	fromaddr = concatstr(3, listname, "+bounces-help@", listfqdn);

	fromstr = headerstr("From: ", helpaddr);
	fputs(fromstr, queuefile);
	free(helpaddr);

	tostr = headerstr("To: ", subaddr);
	fputs(tostr, queuefile);

	subjectstr = headerstr("Subject: Welcome to ", listaddr);
	fputs(subjectstr, queuefile);
	fputc('\n', queuefile);

	while((bufres = fgets(buf, READ_BUFSIZE, subtextfile)))
		if(strncmp(buf, "*LSTADDR*", 9) == 0)
			fputs(listaddr, queuefile);
		else
			fputs(buf, queuefile);

	free(tostr);
	free(subjectstr);
	free(listname);
	free(listfqdn);
	fclose(subtextfile);
	fclose(queuefile);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-m", queuefilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void generate_subconfirm(const char *listdir, const char *listaddr,
			 const char *subaddr, const char *mlmmjsend)
{
	FILE *subconffile, *subtextfile, *queuefile;
	char buf[READ_BUFSIZE];
	char *confirmaddr, *bufres, *listname, *listfqdn, *confirmfilename;
	char *subtextfilename, *queuefilename, *fromaddr, *randomstr;
	char *tostr, *fromstr, *helpaddr, *subjectstr;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	randomstr = random_plus_addr(subaddr);
	confirmfilename = concatstr(3, listdir, "/subconf/", randomstr);

	if((subconffile = fopen(confirmfilename, "w")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", confirmfilename);
		free(confirmfilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	fputs(subaddr, subconffile);
	fclose(subconffile);
	free(confirmfilename);

	helpaddr = concatstr(3, listname, "+help@", listfqdn);

	confirmaddr = concatstr(5, listname, "+confsub-", randomstr, "@",
			           listfqdn);

	fromaddr = concatstr(5, listname, "+bounces-confsub-", randomstr,
				"@", listfqdn);

	subtextfilename = concatstr(2, listdir, "/text/sub-confirm");

	if((subtextfile = fopen(subtextfilename, "r")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", subtextfilename);
		free(randomstr);
		free(subtextfilename);
		exit(EXIT_FAILURE);
	}
	free(subtextfilename);

	queuefilename = concatstr(3, listdir, "/queue/", randomstr);

	printf("%s\n", queuefilename);

	if((queuefile = fopen(queuefilename, "w")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		free(queuefilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	free(randomstr);

	fromstr = headerstr("From: ", helpaddr);
	fputs(fromstr, queuefile);
	free(fromstr);

	tostr = headerstr("To: ", subaddr);
	fputs(tostr, queuefile);
	free(tostr);

	subjectstr = headerstr("Subject: Confirm subscribe to ", listaddr);
	fputs(subjectstr, queuefile);
	fputc('\n', queuefile);
	free(subjectstr);

	while((bufres = fgets(buf, sizeof(buf), subtextfile)))
		if(strncmp(buf, "*LSTADDR*", 9) == 0)
			fputs(listaddr, queuefile);
		else if(strncmp(buf, "*SUBADDR*", 9) == 0)
			fputs(subaddr, queuefile);
		else if(strncmp(buf, "*CNFADDR*", 9) == 0)
			fputs(confirmaddr, queuefile);
		else
			fputs(buf, queuefile);

	free(listname);
	free(listfqdn);
	fclose(subtextfile);
	fclose(queuefile);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-R", confirmaddr,
				"-m", queuefilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/chat-list\n"
	       "          -a someguy@somewhere.ltd\n"
	       "          -C request mail confirmation\n"
	       "          -c send welcome mail\n"
	       "          -h this help\n"
	       "          -V print version\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *listaddr, *listdir = NULL, *address = NULL, *subfilename = NULL;
	char *mlmmjsend, *argv0 = strdup(argv[0]);
	int subconfirm = 0, confirmsub = 0, opt, subfilefd, lock;
	size_t len;
	off_t suboff;

	mlmmjsend = concatstr(2, dirname(argv0), "/mlmmj-send");
	free(argv0);
	
	log_set_name(argv[0]);

	while ((opt = getopt(argc, argv, "hcCVL:a:")) != -1) {
		switch(opt) {
		case 'a':
			address = optarg;
			break;
		case 'c':
			confirmsub = 1;
			break;
		case 'C':
			subconfirm = 1;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'V':
			print_version(argv[0]);
			exit(0);
		}
	}
	if(listdir == 0 || address == 0) {
		fprintf(stderr, "You have to specify -L and -a\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(confirmsub && subconfirm) {
		fprintf(stderr, "Cannot specify both -C and -c\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* get the list address */
	listaddr = getlistaddr(listdir);
	if(strncasecmp(listaddr, address, strlen(listaddr)) == 0) {
		printf("Cannot subscribe the list address to the list\n");
		exit(EXIT_SUCCESS);  /* XXX is this success? */
	}

	subfilename = concatstr(2, listdir, "/subscribers");

	subfilefd = open(subfilename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if(subfilefd == -1) {
		log_error(LOG_ARGS, "Could not open '%s'", subfilename);
		exit(EXIT_FAILURE);
	}

	lock = myexcllock(subfilefd);
	if(lock) {
		log_error(LOG_ARGS, "Error locking subscriber file");
		close(subfilefd);
		exit(EXIT_FAILURE);
	}
	suboff = find_subscriber(subfilefd, address);
	if(suboff == -1) {
		if(subconfirm)
			generate_subconfirm(listdir, listaddr, address,
					    mlmmjsend);
		else {
			lseek(subfilefd, 0L, SEEK_END);
			len = strlen(address);
			address[len] = '\n';
			writen(subfilefd, address, len + 1);
			address[len] = 0;
		}
	} else {
		myunlock(subfilefd);
		free(subfilename);
		close(subfilefd);
		
		return EXIT_SUCCESS;
	}

	if(confirmsub)
		confirm_sub(listdir, listaddr, address, mlmmjsend);

	return EXIT_SUCCESS;
}
