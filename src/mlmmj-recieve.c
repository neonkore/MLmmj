/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <libgen.h>

#include "mlmmj.h"
#include "wrappers.h"
#include "mygetline.h"
#include "strgen.h"
#include "log_error.h"

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
	pid_t childpid;

	CHECKFULLPATH(argv[0]);
	
	log_set_name(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjprocess = concatstr(2, bindir, "/mlmmj-process");
	free(bindir);
	
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
	
	infilename = concatstr(3, listdir, "/incoming/", randomstr);
	free(randomstr);
	fd = open(infilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	while(fd < 0 && errno == EEXIST) {
		free(infilename);
		randomstr = random_str();
		infilename = concatstr(3, listdir, "/incoming/", randomstr);
		free(randomstr);
		fd = open(infilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	}

	if(fd < 0) {
		log_error(LOG_ARGS, "could not create mail file in "
				    "%s/incoming directory", listdir);
		free(infilename);
		exit(EXIT_FAILURE);
	}
	
	if(dumpfd2fd(fileno(stdin), fd) != 0) {
		log_error(LOG_ARGS, "Could not recieve mail");
		exit(EXIT_FAILURE);
	}

	fsync(fd);

#if 0
	log_error(LOG_ARGS, "mlmmj-recieve: wrote %s\n", infilename);
#endif
	close(fd);

	if(noprocess) {
		free(infilename);
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
				"-m", infilename, 0);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjprocess);

	exit(EXIT_FAILURE);
}
