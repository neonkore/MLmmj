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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

#include "mlmmj.h"
#include "wrappers.h"
#include "mygetline.h"
#include "strgen.h"
#include "log_error.h"
#include "memory.h"

extern char *optarg;

static void print_help(const char *prg)
{
        printf("Usage: %s -L /path/to/listdir [-h] [-V] [-P] [-F]\n"
	       " -h: This help\n"
	       " -F: Don't fork in the background\n"
	       " -L: Full path to list directory\n"
	       " -P: Don't execute mlmmj-process\n"
	       " -V: Print version\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *infilename = NULL, *listdir = NULL;
	char *randomstr = random_str();
	char *mlmmjprocess, *bindir;
	int fd, opt, noprocess = 0, nofork = 0;
	struct stat st;
	uid_t uid;
	pid_t childpid;

	CHECKFULLPATH(argv[0]);
	
	log_set_name(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjprocess = concatstr(2, bindir, "/mlmmj-process");
	myfree(bindir);
	
	while ((opt = getopt(argc, argv, "hPVL:F")) != -1) {
		switch(opt) {
		case 'h':
			print_help(argv[0]);
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'P':
			noprocess = 1;
			break;
		case 'F':
			nofork = 1;
			break;
		case 'V':
			print_version(argv[0]);
			exit(0);
		}
	}

	if(listdir == NULL) {
		fprintf(stderr, "You have to specify -L\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Lets make sure no random user tries to send mail to the list */
	if(listdir) {
		if(stat(listdir, &st) == 0) {
			uid = getuid();
			if(uid && uid != st.st_uid) {
				log_error(LOG_ARGS,
					"Have to invoke either as root "
					"or as the user owning listdir "
					"Invoked with uid = [%d]", (int)uid);
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
	
	infilename = concatstr(3, listdir, "/incoming/", randomstr);
	myfree(randomstr);
	fd = open(infilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	while(fd < 0 && errno == EEXIST) {
		myfree(infilename);
		randomstr = random_str();
		infilename = concatstr(3, listdir, "/incoming/", randomstr);
		myfree(randomstr);
		fd = open(infilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	}

	if(fd < 0) {
		log_error(LOG_ARGS, "could not create mail file in "
				    "%s/incoming directory", listdir);
		myfree(infilename);
		exit(EXIT_FAILURE);
	}
	
	if(dumpfd2fd(fileno(stdin), fd) != 0) {
		log_error(LOG_ARGS, "Could not recieve mail");
		exit(EXIT_FAILURE);
	}

#if 0
	log_oper(listdir, OPLOGFNAME, "mlmmj-recieve got %s", infilename);
#endif
	fsync(fd);
	close(fd);

	if(noprocess) {
		myfree(infilename);
		exit(EXIT_SUCCESS);
	}

	/*
	 * Now we fork so we can exit with success since it could potentially
	 * take a long time for mlmmj-send to finish delivering the mails and
	 * returning, making it susceptible to getting a SIGKILL from the
	 * mailserver invoking mlmmj-recieve.
	 */
	if (!nofork) {
		childpid = fork();
		if(childpid < 0)
			log_error(LOG_ARGS, "fork() failed! Proceeding anyway");
	
		if(childpid)
			exit(EXIT_SUCCESS); /* Parent says: "bye bye kids!"*/
	}

	execlp(mlmmjprocess, mlmmjprocess,
				"-L", listdir,
				"-m", infilename, NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjprocess);

	exit(EXIT_FAILURE);
}
