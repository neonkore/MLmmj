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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>

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
	int subtextfd, queuefd;
	char *buf, *subtextfilename, *randomstr, *queuefilename;
	char *fromaddr, *listname, *listfqdn, *s1;

	subtextfilename = concatstr(2, listdir, "/text/unsub-ok");

	if((subtextfd = open(subtextfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", subtextfilename);
		free(subtextfilename);
		exit(EXIT_FAILURE);
	}
	free(subtextfilename);

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	randomstr = random_str();

	queuefilename = concatstr(3, listdir, "/queue/", randomstr);

	if((queuefd = open(queuefilename, O_WRONLY|O_CREAT|O_EXCL,
					S_IRUSR|S_IWUSR)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		free(queuefilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	free(randomstr);

	fromaddr = concatstr(3, listname, "+bounces-help@", listfqdn);

	s1 = concatstr(9, "From: ", listname, "+help@", listfqdn, "\nTo: ",
			subaddr, "\nSubject: Goodbye from ", listaddr,
			"\n\n");

	if(writen(queuefd, s1, strlen(s1)) < 0) {
		log_error(LOG_ARGS, "Could not write subconffile");
		exit(EXIT_FAILURE);
	}

	free(s1);

	while((buf = mygetline(subtextfd))) {
		if(strncmp(buf, "*LSTADDR*", 9) == 0) {
			if(writen(queuefd, listaddr, strlen(listaddr)) < 0) {
				log_error(LOG_ARGS,
					"Could not write goodbye mail");
				exit(EXIT_FAILURE);
			}
		} else {
			if(writen(queuefd, buf, strlen(buf)) < 0) {
				log_error(LOG_ARGS,
					"Could not write goodbye mail");
				exit(EXIT_FAILURE);
			}
		}
	}

	free(listname);
	free(listfqdn);
	close(subtextfd);
	close(queuefd);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-m", queuefilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void generate_unsubconfirm(const char *listdir, const char *listaddr,
			   const char *subaddr, const char *mlmmjsend)
{
	char *confirmaddr, *buf, *listname, *listfqdn, *confirmfilename;
	char *subtextfilename, *queuefilename, *fromaddr, *randomstr, *s1;
	int subconffd, subtextfd, queuefd;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	randomstr = random_plus_addr(subaddr);
	confirmfilename = concatstr(3, listdir, "/unsubconf/", randomstr);

	if((subconffd = open(confirmfilename, O_WRONLY|O_CREAT|O_EXCL,
					S_IRUSR|S_IWUSR)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", confirmfilename);
		free(confirmfilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}

	free(confirmfilename);

	if(writen(subconffd, subaddr, strlen(subaddr)) < 0) {
		log_error(LOG_ARGS, "Could not write unsubconffile");
		exit(EXIT_FAILURE);
	}

	close(subconffd);

	confirmaddr = concatstr(5, listname, "+confunsub-", randomstr, "@",
			           listfqdn);
	fromaddr = concatstr(5, listname, "+bounces-confunsub-", randomstr,
				"@", listfqdn);
	subtextfilename = concatstr(2, listdir, "/text/unsub-confirm");

	if((subtextfd = open(subtextfilename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", subtextfilename);
		free(randomstr);
		free(subtextfilename);
		exit(EXIT_FAILURE);
	}
	free(subtextfilename);

	queuefilename = concatstr(3, listdir, "/queue/", randomstr);

	if((queuefd = open(queuefilename, O_WRONLY|O_CREAT|O_EXCL,
					S_IRUSR|S_IWUSR)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", queuefilename);
		free(queuefilename);
		free(randomstr);
		exit(EXIT_FAILURE);
	}
	free(randomstr);

	s1 = concatstr(9, "From: ", listname, "+help@", listfqdn, "\nTo: ",
			subaddr, "\nSubject: Confirm unsubscribe from ",
			listaddr, "\n\n");

	if(writen(queuefd, s1, strlen(s1)) < 0) {
		log_error(LOG_ARGS, "Could not write subconffile");
		exit(EXIT_FAILURE);
	}

	free(s1);

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

	free(listname);
	free(listfqdn);
	close(subtextfd);
	close(queuefd);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-T", subaddr,
				"-F", fromaddr,
				"-R", confirmaddr,
				"-m", queuefilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

ssize_t unsubscribe(int subreadfd, int subwritefd, const char *address)
{
	off_t suboff = find_subscriber(subreadfd, address);
	struct stat st;
	char *inmap;
	size_t len = strlen(address) + 1; /* + 1 for the '\n' */
	ssize_t writeres, written = 0;
	
	if(suboff == -1)
		return 1; /* Did not find subscriber */

	if(fstat(subreadfd, &st) < 0) {
		log_error(LOG_ARGS, "Could not stat fd");
		return 1;
	}

	if((inmap = mmap(0, st.st_size, PROT_READ, MAP_SHARED,
		       subreadfd, 0)) == MAP_FAILED) {
		log_error(LOG_ARGS, "Could not mmap fd");
		return 1;
	}

	if((writeres = writen(subwritefd, inmap, suboff)) < 0)
		return -1;
	written += writeres;
	
	if((writeres = writen(subwritefd, inmap + suboff + len,
					st.st_size - len - suboff) < 0))
		return -1;
	written += writeres;

	munmap(inmap, st.st_size);

	return written;
}

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/list -a john@doe.org "
	       "[-c] [-C] [-h] [-L] [-V]\n"
	       " -a: Email address to unsubscribe \n"
	       " -c: Send goodbye mail\n"
	       " -C: Request mail confirmation\n"
	       " -h: This help\n"
	       " -L: Full path to list directory\n"
	       " -V: Print version\n"
	       "When no options are specified, unsubscription silently "
	       "happens\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int subread, subwrite, rlock, wlock, opt, unsubres;
	int confirmunsub = 0, unsubconfirm = 0;
	char *listaddr, *listdir = NULL, *address = NULL, *subreadname = NULL;
	char *subwritename, *mlmmjsend, *bindir;
	char *subddirname;
	off_t suboff;
	DIR *subddir;
	struct dirent *dp;

	CHECKFULLPATH(argv[0]);
	
	log_set_name(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjsend = concatstr(2, bindir, "/mlmmj-send");
	free(bindir);

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
	listaddr = getlistaddr(listdir);

	if(unsubconfirm)
		generate_unsubconfirm(listdir, listaddr, address, mlmmjsend);

	subddirname = concatstr(2, listdir, "/subscribers.d/");
	if((subddir = opendir(subddirname)) == NULL) {
		log_error(LOG_ARGS, "Could not opendir(%s)",
				    subddirname);
		free(subddirname);
		exit(EXIT_FAILURE);
	}
	free(subddirname);
	while((dp = readdir(subddir)) != NULL) {
		if(!strcmp(dp->d_name, "."))
			continue;
		if(!strcmp(dp->d_name, ".."))
			continue;
		subreadname = concatstr(3, listdir, "/subscribers.d/",
				dp->d_name);

		subread = open(subreadname, O_RDWR);
		if(subread == -1) {
			log_error(LOG_ARGS, "Could not open '%s'", subreadname);
			free(subreadname);
			continue;
		}

		suboff = find_subscriber(subread, address);
		if(suboff == -1) {
			close(subread);
			free(subreadname);
			continue;
		}

		rlock = myexcllock(subread);
		if(rlock < 0) {
			log_error(LOG_ARGS, "Error locking '%s' file",
					subreadname);
			close(subread);
			free(subreadname);
			continue;
		}

		subwritename = concatstr(2, subreadname, ".new");

		subwrite = open(subwritename, O_RDWR | O_CREAT | O_EXCL,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if(subwrite == -1){
			log_error(LOG_ARGS, "Could not open '%s'",
					subwritename);
			myunlock(subread);
			close(subread);
			free(subreadname);
			free(subwritename);
			continue;
		}

		wlock = myexcllock(subwrite);
		if(wlock < 0) {
			log_error(LOG_ARGS, "Error locking '%s'",
					subwritename);
			myunlock(subread);
			close(subread);
			close(subwrite);
			free(subreadname);
			free(subwritename);
			continue;
		}

		unsubres = unsubscribe(subread, subwrite, address);
		if(unsubres < 0) {
			myunlock(subread);
			myunlock(subwrite);
			close(subread);
			close(subwrite);
			unlink(subwritename);
			free(subreadname);
			free(subwritename);
			continue;
		}

		unlink(subreadname);

		if(unsubres > 0) {
			if(rename(subwritename, subreadname) < 0) {
				log_error(LOG_ARGS,
					"Could not rename '%s' to '%s'",
					subwritename, subreadname);
				myunlock(subread);
				myunlock(subwrite);
				close(subread);
				close(subwrite);
				free(subreadname);
				free(subwritename);
				continue;
			}
		} else /* unsubres == 0, no subscribers left */
			unlink(subwritename);

		myunlock(subread);
		myunlock(subwrite);
		close(subread);
		close(subwrite);
		free(subreadname);
		free(subwritename);

		if(confirmunsub)
			confirm_unsub(listdir, listaddr, address, mlmmjsend);
	}
	closedir(subddir);

	return EXIT_SUCCESS;
}
