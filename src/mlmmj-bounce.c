/* Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
 *
 * $Id$
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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
#include <string.h>
#include <ctype.h>

#include "getlistaddr.h"
#include "mlmmj.h"
#include "strgen.h"
#include "wrappers.h"
#include "log_error.h"
#include "subscriberfuncs.h"
#include "mygetline.h"
#include "chomp.h"
#include "prepstdreply.h"
#include "memory.h"
#include "find_email_adr.h"
#include "gethdrline.h"

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
			line = mymalloc(len + 1);
			strncpy(line, cur, len);
			line[len] = '\0';
			cur = next + 1;
		} else
	 		continue;

		colon = strchr(line, ':');
		*colon = '\0';
		s = indexstr;
		indexstr = concatstr(4, s, "        ", line, "\n");
		myfree(s);
		myfree(line);
	}
		
	munmap(start, st.st_size);
	close(fd);

	return indexstr;
}

void do_probe(const char *listdir, const char *mlmmjsend, const char *addr)
{
	char *myaddr, *from, *a, *indexstr, *queuefilename, *listaddr;
	char *listfqdn, *listname, *probefile;
	char *maildata[] = { "bouncenumbers", NULL };
	int fd;
	time_t t;

	myaddr = mystrdup(addr);

	listaddr = getlistaddr(listdir);
	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

	from = concatstr(5, listname, "+bounces-probe-", myaddr, "@", listfqdn);

	myfree(listaddr);
	myfree(listfqdn);
	myfree(listname);

	a = strrchr(myaddr, '=');
	if (!a) {
		myfree(myaddr);
		myfree(from);
		log_error(LOG_ARGS, "do_probe(): malformed address");
		exit(EXIT_FAILURE);

	}
	*a = '@';

	indexstr = fetchindexes(addr);
	if(indexstr == NULL) {
		log_error(LOG_ARGS, "Could not fetch bounceindexes");
		exit(EXIT_FAILURE);
	}

	maildata[1] = indexstr;
	queuefilename = prepstdreply(listdir, "bounce-probe", "$listowner$",
					myaddr, NULL, 1, maildata);
	MY_ASSERT(queuefilename);
	myfree(indexstr);

	probefile = concatstr(4, listdir, "/bounce/", addr, "-probe");
	MY_ASSERT(probefile);
	t = time(NULL);
	a = mymalloc(32);
	snprintf(a, 31, "%ld", (long int)t);
	a[31] = '\0';
	unlink(probefile);
	fd = open(probefile, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
	if(fd < 0)
		log_error(LOG_ARGS, "Could not open %s", probefile);
	else
		if(writen(fd, a, strlen(a)) < 0)
			log_error(LOG_ARGS, "Could not write time in probe");

	myfree(probefile);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "5",
				"-L", listdir,
				"-T", myaddr,
				"-F", from,
				"-m", queuefilename, (char *)NULL);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);

	exit(EXIT_FAILURE);
}

char *dsnparseaddr(const char *mailname)
{
	int fd, indsn = 0, i;
	char *line, *linedup, *search, *addr = NULL;
	struct email_container emails = { 0, NULL };

	fd = open(mailname, O_RDONLY);
	if(fd < 0) {
		log_error(LOG_ARGS, "Could not open bounceindexfile %s",
				mailname);
		return NULL;
	}

	while((line = gethdrline(fd))) {
		linedup = mystrdup(line);
		for(i = 0; line[i]; i++)
			linedup[i] = tolower(line[i]);
		search = strstr(linedup, "message/delivery-status");
		myfree(linedup);
		if(search)
			indsn = 1;
		if(indsn) {
			i = strncasecmp(line, "Final-Recipient:", 16);
			if(i == 0) {
				find_email_adr(line, &emails);
				if(emails.emailcount > 0) {
					addr = mystrdup(emails.emaillist[0]);
					for(i = 0; i < emails.emailcount; i++)
						myfree(emails.emaillist[i]);
					myfree(emails.emaillist);	
				} else {
					addr = NULL;
				}
				myfree(line);
				return addr;
			}
		}
		myfree(line);
	}
	
	return NULL;
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
	int opt, fd, dsnbounce = 0;
	char *listdir = NULL, *address = NULL, *number = NULL;
	char *bindir, *mlmmjsend, *savename;
	char *mailname = NULL, *bfilename, *a, *buf;
	size_t len;
	time_t t;
	int probe = 0;
	struct stat st;
	uid_t uid;

	log_set_name(argv[0]);

	CHECKFULLPATH(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjsend = concatstr(2, bindir, "/mlmmj-send");
	myfree(bindir);

	while ((opt = getopt(argc, argv, "hdVL:a:n:m:p")) != -1) {
		switch(opt) {
		case 'L':
			listdir = optarg;
			break;
		case 'a':
			address = optarg;
			break;
		case 'd':
			dsnbounce = 1;
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

	if(listdir == NULL || (address == NULL && dsnbounce == 0)
				|| (number == NULL && probe == 0)) {
		fprintf(stderr,
			"You have to specify -L, -a or -d and -n or -p\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Lets make sure no random user tries to do bouncehandling */
	if(listdir) {
		if(stat(listdir, &st) == 0) {
			uid = getuid();
			if(uid && uid != st.st_uid) {
				log_error(LOG_ARGS,
					"Have to invoke either as root "
					"or as the user owning listdir");
				writen(STDERR_FILENO,
					"Have to invoke either as root "
					"or as the user owning listdir\n", 60);
				exit(EXIT_FAILURE);
			}
		} else {
			log_error(LOG_ARGS, "Could not stat %s", listdir);
			exit(EXIT_FAILURE);
		}
	}

	if(dsnbounce) {
		address = dsnparseaddr(mailname);
		if(address == NULL) {
			/* not parseable, so unlink and clean up */
			if(mailname)
				unlink(mailname);
			exit(EXIT_SUCCESS);
		}
		a = strrchr(address, '@');
		*a = '=';
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
	log_error(LOG_ARGS, "listdir = [%s] address = [%s] number = [%s]", listdir, address, number);
#endif

	/* check if it's sub/unsub requests bouncing, and in that case
	 * simply remove the confirmation file. Variablenames address and
	 * number are a bit misleading in this case due to the different
	 * construction of the sub/unsub confirmation From header.
	 */
	if(strcmp(number, "confsub") == 0) {
		a = concatstr(3, listdir, "/subconf/", address);
		unlink(a);
		myfree(a);
		exit(EXIT_SUCCESS);
	}
	if(strcmp(number, "confunsub") == 0) {
		a = concatstr(3, listdir, "/unsubconf/", address);
		unlink(a);
		myfree(a);
		exit(EXIT_SUCCESS);
	}
	/* Below checks for bounce probes bouncing. If they do, simply remove
	 * the probe file and exit successfully. Yes, I know the variables
	 * have horrible names, but please bear with me.
	 */
	if(strcmp(number, "probe") == 0) {
		a = concatstr(4, listdir, "/bounce/", address, "-probe");
		unlink(a);
		unlink(mailname);
		myfree(a);
		exit(EXIT_SUCCESS);
	}
	
	/* save the filename with '=' before replacing it with '@' */
	bfilename = concatstr(3, listdir, "/bounce/", address);

	a = strrchr(address, '=');
	if (!a)
		exit(EXIT_SUCCESS);  /* ignore malformed address */
	*a = '@';

	/* make sure it's a subscribed address */
	if(is_subbed(listdir, address)) {
		myfree(bfilename);
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
		myfree(bfilename);
		exit(EXIT_FAILURE);
	}

	/* TODO check that the message is not already bounced */

	/* XXX How long can the string representation of an integer be?
	 * It is not a security issue (we use snprintf()), but it would be
	 * bad mojo to cut the timestamp field  -- mortenp 20040427 */

	/* int + ":" + int + " # Wed Jun 30 21:49:08 1993\n" + NUL */
	len = 20 + 1 + 20 + 28 + 1;

	buf = mymalloc(len);
	if (!buf) exit(EXIT_FAILURE);

	t = time(NULL);
	snprintf(buf, len-26, "%s:%ld # ", number, (long int)t);
	ctime_r(&t, buf+strlen(buf));
	writen(fd, buf, strlen(buf));
	close(fd);

	if(mailname) {
		savename = concatstr(2, bfilename, ".lastmsg");
		rename(mailname, savename);
		myfree(savename);
	}
		
	myfree(bfilename);
	if(dsnbounce && address)
		myfree(address);

	return EXIT_SUCCESS;
}
