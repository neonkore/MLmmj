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

#include "mlmmj.h"
#include "mlmmj-subscribe.h"
#include "mylocking.h"
#include "wrappers.h"
#include "getlistaddr.h"
#include "strgen.h"
#include "subscriberfuncs.h"

#include "log_error.c"

void confirm_sub(const char *listdir, const char *listaddr, const char *subaddr)
{
	size_t len;
	FILE *subtextfile;
	FILE *queuefile;
	char buf[READ_BUFSIZE];
	char *bufres;
	char *subtextfilename;
	char *randomstr;
	char *queuefilename;
	char *fromstr;
	char *tostr;
	char *subjectstr;
	char *fromaddr;
	char *helpaddr;
	char *listname;
	char *listfqdn;

	subtextfilename = genfilename(listdir, "/text/sub-ok");

	if((subtextfile = fopen(subtextfilename, "r")) == NULL) {
		log_error("Could not open text/sub-confirm\n");
		free(subtextfilename);
		exit(EXIT_FAILURE);
	}
	free(subtextfilename);

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	randomstr = random_str();

	queuefilename = gendirname(listdir, "/queue/", randomstr);

	printf("%s\n", queuefilename);

	if((queuefile = fopen(queuefilename, "w")) == NULL) {
		log_error(queuefilename);
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

	subjectstr = headerstr("Subject: Welcome to ", listaddr);
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

	execlp(BINDIR"mlmmj-send", "mlmmj-send",
				"-L", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-m", queuefilename, 0);
	fprintf(stderr, "%s:%d execlp() of "BINDIR"mlmmj-send failed: ", __FILE__, __LINE__);
	perror(NULL);
	exit(EXIT_FAILURE);
}

void generate_subconfirm(const char *listdir, const char *listaddr,
				const char *subaddr)
{
	size_t len;
	char *confirmaddr;
	char buf[READ_BUFSIZE];
	char *bufres;
	char *listname;
	char *listfqdn;
	char *confirmfilename;
	char *subtextfilename;
	char *queuefilename;
	char *fromaddr;
	char *randomstr;
	char *tostr;
	char *fromstr;
	char *helpaddr;
	char *subjectstr;
	FILE *subconffile;
	FILE *subtextfile;
	FILE *queuefile;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	randomstr = random_plus_addr(subaddr);
	confirmfilename = gendirname(listdir, "/subconf/", randomstr);

	if((subconffile = fopen(confirmfilename, "w")) == NULL) {
		log_error(confirmfilename);
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

	len = strlen(listname) + strlen(listfqdn) + strlen("+confsub") 
				+ strlen(subaddr) + 20;
	confirmaddr = malloc(len);
	snprintf(confirmaddr, len, "%s+confsub-%s@%s", listname, randomstr,
							listfqdn);

	len += strlen("-bounces");
	fromaddr = malloc(len);
	snprintf(fromaddr, len, "%s-bounces+confsub-%s@%s", listname,
			randomstr, listfqdn);

	subtextfilename = genfilename(listdir, "/text/sub-confirm");

	if((subtextfile = fopen(subtextfilename, "r")) == NULL) {
		log_error("Could not open text/sub-confirm\n");
		free(randomstr);
		free(subtextfilename);
		exit(EXIT_FAILURE);
	}
	free(subtextfilename);

	queuefilename = gendirname(listdir, "/queue/", randomstr);

	printf("%s\n", queuefilename);

	if((queuefile = fopen(queuefilename, "w")) == NULL) {
		log_error(queuefilename);
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

	execlp(BINDIR"mlmmj-send", "mlmmj-send",
				"-L", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-R", confirmaddr,
				"-m", queuefilename, 0);
	fprintf(stderr, "%s:%d execlp() of "BINDIR"mlmmj-send failed: ", __FILE__, __LINE__);
	perror(NULL);
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
	size_t len;
	int opt, subfilefd, lock;
	char listaddr[READ_BUFSIZE];
	char *listdir = 0;
	char *address = 0;
	char *subfilename = 0;
	int subconfirm = 0;
	int confirmsub = 0;

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
	getlistaddr(listaddr, listdir);
	if(strncasecmp(listaddr, address, strlen(listaddr)) == 0) {
		printf("Cannot subscribe the list address to the list\n");
		exit(EXIT_SUCCESS);  /* XXX is this success? */
	}

	subfilename = genfilename(listdir, "/subscribers");

	subfilefd = open(subfilename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if(subfilefd == -1) {
		log_error("Could not open subscriberfile:");
		exit(EXIT_FAILURE);
	}

	lock = myexcllock(subfilefd);
	if(lock) {
		log_error("Error locking subscriber file:");
		close(subfilefd);
		exit(EXIT_FAILURE);
	}

	if(find_subscriber(subfilefd, address)) {
		if(subconfirm)
			generate_subconfirm(listdir, listaddr, address);
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
		confirm_sub(listdir, listaddr, address);

	return EXIT_SUCCESS;
}
