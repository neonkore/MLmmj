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
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#include "mlmmj.h"
#include "mlmmj-unsub.h"
#include "mylocking.h"
#include "wrappers.h"
#include "mygetline.h"
#include "getlistaddr.h"
#include "subscriberfuncs.h"
#include "strgen.h"
#include "log_error.h"

void confirm_unsub(const char *listdir, const char *listaddr,
		   const char *subaddr, const char *mlmmjsend)
{
	size_t len;
	FILE *subtextfile;
	FILE *queuefile;
	char buf[READ_BUFSIZE];
	char *bufres, *subtextfilename, *randomstr, *queuefilename;
	char *fromstr, *tostr, *subjectstr, *fromaddr, *helpaddr;
	char *listname, *listfqdn;

	subtextfilename = concatstr(2, listdir, "/text/unsub-ok");

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

	len = strlen(listname) + strlen(listfqdn) + strlen("+help@") + 1;
	helpaddr = malloc(len);
	snprintf(helpaddr, len, "%s+help@%s", listname, listfqdn);

	len += strlen("-bounces");
	fromaddr = malloc(len);
	snprintf(fromaddr, len, "%s-bounces+help@%s", listname, listfqdn);

	fromstr = headerstr("From: ", helpaddr);
	fputs(fromstr, queuefile);
	free(helpaddr);

	tostr = headerstr("To: ", subaddr);
	fputs(tostr, queuefile);

	subjectstr = headerstr("Subject: Goodbye from ", listaddr);
	fputs(subjectstr, queuefile);
	fputc('\n', queuefile);

	while((bufres = fgets(buf, READ_BUFSIZE, subtextfile)))
		if(strncmp(buf, "*LSTADDR*", 9) == 0)
			fputs(listaddr, queuefile);
		else
			fputs(buf, queuefile);
#ifdef MLMMJ_DEBUG
	fprintf(stderr, "subaddr: [%s]", subaddr);
	fprintf(stderr, "[%s]", fromstr);
	fprintf(stderr, "[%s]", tostr);
	fprintf(stderr, "[%s]", subjectstr);
#endif
	free(tostr);
	free(subjectstr);
	free(listname);
	free(listfqdn);
	fclose(subtextfile);
	fclose(queuefile);

	execlp(mlmmjsend, mlmmjsend,
				"-L", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-m", queuefilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void generate_unsubconfirm(const char *listdir, const char *listaddr,
			   const char *subaddr, const char *mlmmjsend)
{
	size_t len;
	char buf[READ_BUFSIZE];
	char *confirmaddr, *bufres, *listname, *listfqdn, *confirmfilename;
	char *subtextfilename, *queuefilename, *fromaddr, *randomstr;
	char *tostr, *fromstr, *helpaddr, *subjectstr;
	FILE *subconffile, *subtextfile, *queuefile;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	randomstr = random_plus_addr(subaddr);
	confirmfilename = concatstr(3, listdir, "/unsubconf/", randomstr);

	if((subconffile = fopen(confirmfilename, "w")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", confirmfilename);
		free(confirmfilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	fputs(subaddr, subconffile);
	fclose(subconffile);
	free(confirmfilename);

	len = strlen(listname) + strlen(listfqdn) + strlen("+help@") + 1;
	helpaddr = malloc(len);
	snprintf(helpaddr, len, "%s+help@%s", listname, listfqdn);

	len = strlen(listname) + strlen(listfqdn) + strlen("+confunsub") 
				+ strlen(subaddr) + 20;
	confirmaddr = malloc(len);
	snprintf(confirmaddr, len, "%s+confunsub-%s@%s", listname, randomstr,
							listfqdn);

	len += strlen("-bounces");
	fromaddr = malloc(len);
	snprintf(fromaddr, len, "%s-bounces+confunsub-%s@%s", listname,
			randomstr, listfqdn);

	subtextfilename = concatstr(2, listdir, "/text/unsub-confirm");

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

	subjectstr = headerstr("Subject: Confirm unsubscribe from ", listaddr);
	fputs(subjectstr, queuefile);
	fputc('\n', queuefile);
	free(subjectstr);

	while((bufres = fgets(buf, READ_BUFSIZE, subtextfile)))
		if(strncmp(buf, "*LSTADDR*", 9) == 0)
			fputs(listaddr, queuefile);
		else if(strncmp(buf, "*SUBADDR*", 9) == 0)
			fputs(subaddr, queuefile);
		else if(strncmp(buf, "*CNFADDR*", 9) == 0)
			fputs(confirmaddr, queuefile);
		else
			fputs(buf, queuefile);
#ifdef MLMMJ_DEBUG
	fprintf(stderr, "[%s]", fromstr);
	fprintf(stderr, "[%s]", tostr);
	fprintf(stderr, "[%s]", subjectstr);
#endif
	free(listname);
	free(listfqdn);
	fclose(subtextfile);
	fclose(queuefile);

	execlp(mlmmjsend, mlmmjsend,
				"-L", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-R", confirmaddr,
				"-m", queuefilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void unsubscribe(int subreadfd, int subwritefd, const char *address)
{
	char *buf;

	lseek(subreadfd, 0, SEEK_SET);
	lseek(subwritefd, 0, SEEK_SET);

	while((buf = mygetline(subreadfd))) {
		if(strncasecmp(buf, address, strlen(address)) != 0)
			writen(subwritefd, buf, strlen(buf));
		free(buf);
	}
	ftruncate(subwritefd, lseek(subwritefd, 0, SEEK_CUR));
}

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/chat-list\n"
	       "          -a someguy@somewhere.ltd\n"
	       "          -C request mail confirmation\n"
	       "          -c send goodbye mail\n"
	       "          -h this help\n"
	       "          -V print version\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int subread, subwrite, sublock, opt;
	int confirmunsub = 0, unsubconfirm = 0;
	char listaddr[READ_BUFSIZE];
	char *listdir = NULL, *address = NULL, *subreadname = NULL;
	char *mlmmjsend, *argv0 = strdup(argv[0]);
	
	mlmmjsend = concatstr(2, dirname(argv0), "/mlmmj-send");
	free(argv0);

        log_set_name(argv[0]);

	while ((opt = getopt(argc, argv, "hcCVL:a:")) != -1) {
		switch(opt) {
		case 'L':
			listdir = optarg;
			break;
		case 'a':
			address = optarg;
			break;
		case 'c':
			confirmunsub = 1;
			break;
		case 'C':
			unsubconfirm = 1;
			break;
		case 'h':
			print_help(argv[0]);
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

	if(confirmunsub && unsubconfirm) {
		fprintf(stderr, "Cannot specify both -C and -c\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* get the list address */
	getlistaddr(listaddr, listdir);

	subreadname = concatstr(2, listdir, "/subscribers");

	subread = open(subreadname, O_RDWR);
	if(subread == -1) {
		log_error(LOG_ARGS, "Could not open '%s'", subreadname);
		exit(EXIT_FAILURE);
	}

	sublock = myexcllock(subread);
	if(sublock) {
		log_error(LOG_ARGS, "Error locking subscriber file");
		close(subread);
		exit(EXIT_FAILURE);
	}

	if(find_subscriber(subread, address)) {
		myunlock(subread);
		free(subreadname);
		close(subread);
		exit(EXIT_SUCCESS);
	}

	subwrite = open(subreadname, O_RDWR);
	if(subwrite == -1){
		log_error(LOG_ARGS, "Could not open '%s'", subreadname);
		exit(EXIT_FAILURE);
	}
	if(unsubconfirm)
		generate_unsubconfirm(listdir, listaddr, address, mlmmjsend);
	else
		unsubscribe(subread, subwrite, address);
	
	myunlock(subread);

	free(subreadname);
	close(subread);
	close(subwrite);

	if(confirmunsub)
		confirm_unsub(listdir, listaddr, address, mlmmjsend);

	return EXIT_SUCCESS;
}
