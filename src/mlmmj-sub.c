/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <sys/wait.h>

#include "mlmmj.h"
#include "mlmmj-sub.h"
#include "mylocking.h"
#include "wrappers.h"
#include "getlistaddr.h"
#include "strgen.h"
#include "subscriberfuncs.h"
#include "log_error.h"
#include "mygetline.h"
#include "statctrl.h"
#include "prepstdreply.h"
#include "memory.h"

void confirm_sub(const char *listdir, const char *listaddr,
		const char *subaddr, const char *mlmmjsend,
		int digest)
{
	int subtextfd, queuefd;
	char *buf, *subtextfilename, *randomstr, *queuefilename = NULL;
	char *fromaddr, *listname, *listfqdn, *s1;

	if (!digest) {
		subtextfilename = concatstr(2, listdir, "/text/sub-ok");
	} else {
		subtextfilename = concatstr(2, listdir, "/text/sub-ok-digest");
	}

	if((subtextfd = open(subtextfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", subtextfilename);
		myfree(subtextfilename);
		exit(EXIT_FAILURE);
	}
	myfree(subtextfilename);

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

	do {
		randomstr = random_str();
		myfree(queuefilename);
		queuefilename = concatstr(3, listdir, "/queue/", randomstr);
		myfree(randomstr);

		queuefd = open(queuefilename, O_RDWR|O_CREAT|O_EXCL,
					      S_IRUSR|S_IWUSR);

	} while ((queuefd < 0) && (errno == EEXIST));

	if(queuefd < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		myfree(queuefilename);
		exit(EXIT_FAILURE);
	}

	fromaddr = concatstr(3, listname, "+bounces-help@", listfqdn);

	if (!digest) {
		s1 = concatstr(9, "From: ", listname, "+help@", listfqdn,
				"\nTo: ", subaddr, "\nSubject: Welcome to ",
				listaddr, "\n\n");
	} else {
		s1 = concatstr(9, "From: ", listname, "+help@", listfqdn,
				"\nTo: ", subaddr, "\nSubject: Welcome to "
				"digest of ", listaddr, "\n\n");
	}

	if(writen(queuefd, s1, strlen(s1)) < 0) {
		log_error(LOG_ARGS, "Could not write welcome mail");
		exit(EXIT_FAILURE);
	}
	myfree(s1);

	while((buf = mygetline(subtextfd))) {
		if(strncmp(buf, "*LSTADDR*", 9) == 0) {
			if(writen(queuefd, listaddr, strlen(listaddr)) < 0) {
				log_error(LOG_ARGS, "Could not write welcome"
						" mail");
				exit(EXIT_FAILURE);
			}
		} else {
			if(writen(queuefd, buf, strlen(buf)) < 0) {
				log_error(LOG_ARGS, "Could not write welcome"
						" mail");
				exit(EXIT_FAILURE);
			}
		}
		myfree(buf);
	}

	myfree(listname);
	myfree(listfqdn);
	close(subtextfd);
	close(queuefd);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-m", queuefilename, NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void notify_sub(const char *listdir, const char *listaddr,
		const char *subaddr, const char *mlmmjsend,
		int digest)
{
	char *maildata[4] = { "*LSTADDR*", NULL, "*SUBADDR*", NULL };
	char *listfqdn, *listname, *fromaddr, *fromstr, *subject;
	char *queuefilename = NULL;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	maildata[1] = mystrdup(listaddr);
	maildata[3] = mystrdup(subaddr);
	fromaddr = concatstr(3, listname, "+bounces-help@", listfqdn);
	fromstr = concatstr(3, listname, "+owner@", listfqdn);
	if (!digest) {
		subject = concatstr(2, "New subscription to ", listaddr);
		queuefilename = prepstdreply(listdir, "notifysub", fromstr,
						fromstr, NULL, subject, 2,
						maildata);
	} else {
		subject = concatstr(2, "New subscription to digest of ",
					listaddr);
		queuefilename = prepstdreply(listdir, "notifysub-digest",
						fromstr, fromstr, NULL,
						subject, 2, maildata);
	}
	MY_ASSERT(queuefilename)
	myfree(listname);
	myfree(listfqdn);
	myfree(subject);
	myfree(maildata[1]);
	myfree(maildata[3]);
	execlp(mlmmjsend, mlmmjsend,
			"-l", "1",
			"-T", fromstr,
			"-F", fromaddr,
			"-m", queuefilename, NULL);

	myfree(fromstr);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void generate_subconfirm(const char *listdir, const char *listaddr,
			 const char *subaddr, const char *mlmmjsend,
			 int digest)
{
	int subconffd, subtextfd, queuefd;
	char *confirmaddr, *listname, *listfqdn, *confirmfilename = NULL;
	char *subtextfilename, *queuefilename = NULL, *fromaddr;
	char *buf, *s1, *randomstr = NULL;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

        do {
                myfree(confirmfilename);
                myfree(randomstr);
		randomstr = random_plus_addr(subaddr);
                confirmfilename = concatstr(3, listdir, "/subconf/",
					    randomstr);

                subconffd = open(confirmfilename, O_RDWR|O_CREAT|O_EXCL,
						  S_IRUSR|S_IWUSR);

        } while ((subconffd < 0) && (errno == EEXIST));

	if(subconffd < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", confirmfilename);
		myfree(confirmfilename);
                myfree(randomstr);
		exit(EXIT_FAILURE);
	}

	myfree(confirmfilename);

	if(writen(subconffd, subaddr, strlen(subaddr)) < 0) {
		log_error(LOG_ARGS, "Could not write to subconffd");
		myfree(confirmfilename);
                myfree(randomstr);
		exit(EXIT_FAILURE);
	}

	close(subconffd);

	fromaddr = concatstr(5, listname, "+bounces-confsub-", randomstr,
				"@", listfqdn);

	if (!digest) {
		subtextfilename = concatstr(2, listdir, "/text/sub-confirm");
		confirmaddr = concatstr(5, listname, "+confsub-",
					randomstr, "@", listfqdn);
	} else {
		subtextfilename = concatstr(2, listdir, "/text/sub-confirm-digest");
		confirmaddr = concatstr(5, listname, "+confsub-digest-",
					randomstr, "@", listfqdn);
	}

	myfree(randomstr);

	if((subtextfd = open(subtextfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", subtextfilename);
		myfree(subtextfilename);
		exit(EXIT_FAILURE);
	}

	myfree(subtextfilename);

        do {
                randomstr = random_str();
                myfree(queuefilename);
                queuefilename = concatstr(3, listdir, "/queue/", randomstr);
                myfree(randomstr);

                queuefd = open(queuefilename, O_RDWR|O_CREAT|O_EXCL,
					      S_IRUSR|S_IWUSR);

        } while ((queuefd < 0) && (errno == EEXIST));

	if(queuefd < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		myfree(queuefilename);
		exit(EXIT_FAILURE);
	}

	if (!digest) {
		s1 = concatstr(9, "From: ", listname, "+help@", listfqdn,
			"\nTo: ", subaddr, "\nSubject: Confirm subscribe to ",
			listaddr, "\n\n");
	} else {
		s1 = concatstr(9, "From: ", listname, "+help@", listfqdn,
			"\nTo: ", subaddr, "\nSubject: Confirm subscribe to "
			"digest of ", listaddr, "\n\n");
	}

	if(writen(queuefd, s1, strlen(s1)) < 0) {
		log_error(LOG_ARGS, "Could not write subconffile");
		exit(EXIT_FAILURE);
	}

	myfree(s1);

	while((buf = mygetline(subtextfd))) {
		if(strncmp(buf, "*LSTADDR*", 9) == 0) {
			if(writen(queuefd, listaddr, strlen(listaddr)) < 0) {
				log_error(LOG_ARGS,
					"Could not write subconffile");
				exit(EXIT_FAILURE);
			}
		} else if(strncmp(buf, "*SUBADDR*", 9) == 0) {
			if(writen(queuefd, subaddr, strlen(subaddr)) < 0) {
				log_error(LOG_ARGS,
					"Could not write subconffile");
				exit(EXIT_FAILURE);
			}
		} else if(strncmp(buf, "*CNFADDR*", 9) == 0) {
			if(writen(queuefd, confirmaddr, strlen(confirmaddr))
					< 0) {
				log_error(LOG_ARGS,
					"Could not write subconffile");
				exit(EXIT_FAILURE);
			}
		} else {
			if(writen(queuefd, buf, strlen(buf)) < 0) {
				log_error(LOG_ARGS,
					"Could not write subconffile");
				exit(EXIT_FAILURE);
			}
		}
	}

	myfree(listname);
	myfree(listfqdn);
	close(subtextfd);
	close(queuefd);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-R", confirmaddr,
				"-m", queuefilename, NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/list -a john@doe.org "
	       "[-c] [-C] [-h]\n       [-L] [-U] [-V]\n"
	       " -a: Email address to subscribe \n"
	       " -c: Send welcome mail\n"
	       " -C: Request mail confirmation\n"
	       " -d: Subscribe to digest of list\n"
	       " -h: This help\n"
	       " -L: Full path to list directory\n"
	       " -U: Don't switch to the user id of the listdir owner\n"
	       " -V: Print version\n"
	       "When no options are specified, subscription silently "
	       "happens\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *listaddr, *listdir = NULL, *address = NULL, *subfilename = NULL;
	char *mlmmjsend, *bindir, chstr[2];
	int subconfirm = 0, confirmsub = 0, opt, subfilefd, lock, notifysub;
	int changeuid = 1, status, digest = 0;
	size_t len;
	off_t suboff;
	struct stat st;
	pid_t pid, childpid;
	uid_t uid;

	CHECKFULLPATH(argv[0]);

	log_set_name(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjsend = concatstr(2, bindir, "/mlmmj-send");
	myfree(bindir);

	while ((opt = getopt(argc, argv, "hcCdVL:a:")) != -1) {
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
		case 'd':
			digest = 1;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'U':
			changeuid = 0;
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

	if(changeuid) {
		uid = getuid();
		if(!uid && stat(listdir, &st) == 0) {
			printf("Changing to uid %d, owner of %s.\n",
					(int)st.st_uid, listdir);
			if(setuid(st.st_uid) < 0) {
				perror("setuid");
				fprintf(stderr, "Continuing as uid %d\n",
						(int)uid);
			}
		}
	}

	chstr[0] = address[0];
	chstr[1] = '\0';
	if (!digest) {
		subfilename = concatstr(3, listdir, "/subscribers.d/", chstr);
	} else {
		subfilename = concatstr(3, listdir, "/digesters.d/", chstr);
	}

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
					    mlmmjsend, digest);
		else {
			lseek(subfilefd, 0L, SEEK_END);
			len = strlen(address);
			address[len] = '\n';
			writen(subfilefd, address, len + 1);
			address[len] = 0;
		}
	} else {
		myunlock(subfilefd);
		myfree(subfilename);
		close(subfilefd);
		
		return EXIT_SUCCESS;
	}

	if(confirmsub) {
		childpid = fork();

		if(childpid < 0) {
			log_error(LOG_ARGS, "Could not fork");
			confirm_sub(listdir, listaddr, address, mlmmjsend,
					digest);
		}
		
		if(childpid > 0) {
			do /* Parent waits for the child */
				pid = waitpid(childpid, &status, 0);
			while(pid == -1 && errno == EINTR);
		}

		/* child confirms subscription */
		if(childpid == 0)
			confirm_sub(listdir, listaddr, address, mlmmjsend,
					digest);
	}

	notifysub = statctrl(listdir, "notifysub");

	/* Notify list owner about subscription */
	if (notifysub)
		notify_sub(listdir, listaddr, address, mlmmjsend, digest);

	myfree(listaddr);

	return EXIT_SUCCESS;
}
