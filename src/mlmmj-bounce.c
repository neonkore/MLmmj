/* Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <sys/mman.h>

#include "getlistaddr.h"
#include "mlmmj.h"
#include "strgen.h"
#include "wrappers.h"
#include "log_error.h"
#include "subscriberfuncs.h"
#include "mygetline.h"
#include "chomp.h"
#include "prepstdreply.h"

char *fetchindexes(const char *bouncefile)
{
	int fd;
	char *indexstr = NULL, *s, *start, *line, *cur, *colon, *next;
	size_t len;
	struct stat st;
	
	fd = open(bouncefile, O_RDONLY);
	if(fd < 0) {
		log_error(LOG_ARGS, "Could not open bounceindexfile %s",
				bouncefile);
		return NULL;
	}

	if(fstat(fd, &st) < 0) {
		log_error(LOG_ARGS, "Could not open bounceindexfile %s",
					bouncefile);
	}

	start = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(start == MAP_FAILED) {
		log_error(LOG_ARGS, "Could not mmap bounceindexfile");
		return NULL;
	}
	
	
	for(next = cur = start; next < start + st.st_size; next++) {
		if(*next == '\n') {
			len = next - cur;
			line = malloc(len + 1);
			strncpy(line, cur, len);
			line[len] = '\0';
			cur = next + 1;
		} else
	 		continue;

		colon = strchr(line, ':');
		*colon = '\0';
		s = indexstr;
		indexstr = concatstr(4, s, "        ", line, "\n");
		free(s);
		free(line);
	}
		
	munmap(start, st.st_size);
	close(fd);

	return indexstr;
}

void do_probe(const char *listdir, const char *mlmmjsend, const char *addr)
{
	char *myaddr, *from, *a, *fromstr, *subjectstr, *indexstr;
	char *queuefilename, *listaddr, *listfqdn, *listname, *probefile;
	char *maildata[] = { "*LSTADDR*", NULL, "*BOUNCENUMBERS*", NULL };
	int fd;
	time_t t;

	myaddr = strdup(addr);
	MY_ASSERT(myaddr);

	listaddr = getlistaddr(listdir);
	chomp(listaddr);

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

	from = concatstr(5, listname, "+bounces-", myaddr, "-probe@", listfqdn);

	a = strchr(myaddr, '=');
	if (!a) {
		free(myaddr);
		free(from);
		log_error(LOG_ARGS, "do_probe(): malformed address");
		exit(EXIT_FAILURE);
	}
	*a = '@';

	fromstr = concatstr(3, listname, "+owner@", listfqdn);

	subjectstr = concatstr(3, "Mails to you from ", listaddr,
					" have been bouncing");

	indexstr = fetchindexes(addr);
	if(indexstr == NULL) {
		log_error(LOG_ARGS, "Could not fetch bounceindexes");
		exit(EXIT_FAILURE);
	}

	maildata[1] = listaddr;
	maildata[3] = indexstr;
	queuefilename = prepstdreply(listdir, "bounce-probe", fromstr,
					myaddr, NULL, subjectstr, 2, maildata);
	MY_ASSERT(queuefilename);
	free(fromstr);
	free(subjectstr);
	free(listaddr);
	free(listfqdn);
	free(listname);

	probefile = concatstr(4, listdir, "/bounce/", addr, "-probe");
	MY_ASSERT(probefile);
	t = time(NULL);
	a = malloc(32);
	snprintf(a, 31, "%ld", (long int)t);
	a[31] = '\0';
	unlink(probefile);
	fd = open(probefile, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	if(fd < 0)
		log_error(LOG_ARGS, "Could not open %s", probefile);
	else
		if(writen(fd, a, strlen(a)) < 0)
			log_error(LOG_ARGS, "Could not write time in probe");

	free(probefile);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", myaddr,
				"-F", from,
				"-m", queuefilename, 0);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);

	exit(EXIT_FAILURE);
}

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/list -a john=doe.org [-n num | -p]\n"
		" -a: Address string that bounces\n"
		" -h: This help\n"
		" -L: Full path to list directory\n"
		" -n: Message number in the archive\n"
		" -p: Send out a probe\n"
		" -V: Print version\n", prg);

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int opt, fd;
	char *listdir = NULL, *address = NULL, *number = NULL;
	char *bindir, *mlmmjsend;
	char *mailname = NULL, *savename, *bfilename, *a, *buf;
	size_t len;
	time_t t;
	int probe = 0;
	struct stat st;

	log_set_name(argv[0]);

	CHECKFULLPATH(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjsend = concatstr(2, bindir, "/mlmmj-send");
	free(bindir);

	while ((opt = getopt(argc, argv, "hVL:a:n:m:p")) != -1) {
		switch(opt) {
		case 'L':
			listdir = optarg;
			break;
		case 'a':
			address = optarg;
			break;
		case 'm':
			mailname = optarg;
			break;
		case 'n':
			number = optarg;
			break;
		case 'p':
			probe = 1;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'V':
			print_version(argv[0]);
			exit(0);
		}
	}
	if(listdir == NULL || address == NULL || (number == NULL && probe == 0)) {
		fprintf(stderr, "You have to specify -L, -a and -n or -p\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if(number != NULL && probe != 0) {
		fprintf(stderr, "You can only specify one of -n or -p\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (probe) {
		/* send out a probe */
		do_probe(listdir, mlmmjsend, address);
		/* do_probe() will never return */
		exit(EXIT_FAILURE);
	}

#if 0
	log_error(LOG_ARGS, "[%s] [%s] [%s]", listdir, address, number);
#endif

	/* check if it's sub/unsub requests bouncing, and in that case
	 * simply remove the confirmation file. Variablenames address and
	 * number are a bit misleading in this case due to the different
	 * construction of the sub/unsub confirmation From header.
	 */
	if(strncmp(address, "confsub-", 8) == 0) {
		a = concatstr(5, listdir, "/subconf/", address + 8, "-",
				number);
		unlink(a);
		free(a);
		exit(EXIT_SUCCESS);
	}
	if(strncmp(address, "confunsub-", 10) == 0) {
		a = concatstr(5, listdir, "/unsubconf/", address + 10, "-",
				number);
		unlink(a);
		free(a);
		exit(EXIT_SUCCESS);
	}
	/* Below checks for bounce probes bouncing. If they do, simply remove
	 * the probe file and exit successfully
	 */
	if(strncmp(number, "probe", 5) == 0) {
		a = concatstr(4, listdir, "/bounce/", address, "-probe");
		unlink(a);
		unlink(mailname);
		free(a);
		exit(EXIT_SUCCESS);
	}
	
	/* save the filename with '=' before replacing it with '@' */
	bfilename = concatstr(3, listdir, "/bounce/", address);

	a = strchr(address, '=');
	if (!a)
		exit(EXIT_SUCCESS);  /* ignore malformed address */
	*a = '@';

	/* make sure it's a subscribed address */
	if(is_subbed(listdir, address)) {
		free(bfilename);
		exit(EXIT_SUCCESS); /* Not subbed, so exit silently */
	}

	if(lstat(bfilename, &st) == 0) {
		if((st.st_mode & S_IFLNK) == S_IFLNK) {
			log_error(LOG_ARGS, "%s is a symbolic link",
					bfilename);
			exit(EXIT_FAILURE);
		}
	}

	if ((fd = open(bfilename, O_WRONLY|O_APPEND|O_CREAT,
			S_IRUSR|S_IWUSR)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", bfilename);
		free(bfilename);
		exit(EXIT_FAILURE);
	}
	free(bfilename);

	/* TODO check that the message is not already bounced */

	/* XXX How long can the string representation of an integer be?
	 * It is not a security issue (we use snprintf()), but it would be
	 * bad mojo to cut the timestamp field  -- mortenp 20040427 */

	/* int + ":" + int + " # Wed Jun 30 21:49:08 1993\n" + NUL */
	len = 20 + 1 + 20 + 28 + 1;

	buf = malloc(len);
	if (!buf) exit(EXIT_FAILURE);

	t = time(NULL);
	snprintf(buf, len-26, "%s:%ld # ", number, (long int)t);
	ctime_r(&t, buf+strlen(buf));
	writen(fd, buf, strlen(buf));
	close(fd);

	if(mailname) {
		savename = concatstr(2, bfilename, ".lastmsg");
		rename(mailname, savename);
		free(savename);
	}

	return EXIT_SUCCESS;
}
