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

#include "mlmmj.h"
#include "wrappers.h"
#include "mygetline.h"
#include "strgen.h"

extern char *optarg;

static void print_help(const char *prg)
{
        printf("Usage: %s -L /path/to/chat-list [-V] [-P]\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *infilename = NULL, *listdir = NULL, *line = NULL;
	char *randomstr = random_str();
	int fd, opt, noprocess = 0;
	
	while ((opt = getopt(argc, argv, "hPVL:")) != -1) {
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
		fprintf(stderr, "%s:%d could not get fd in %s: ",
				__FILE__, __LINE__, infilename);
		free(infilename);
		exit(EXIT_FAILURE);
	}
	
	while((line = mygetline(stdin))) {
		writen(fd, line, strlen(line));
		fsync(fd);
		free(line);
	}

	printf("mlmmj-recieve: wrote %s\n", infilename);

	close(fd);

	if(noprocess) {
		free(infilename);
		exit(EXIT_SUCCESS);
	}

	execlp("mlmmj-process", "mlmmj-process",
				"-L", listdir,
				"-m", infilename, 0);

	fprintf(stderr, "%s:%d execlp() of mlmmj-process failed: ",
			__FILE__, __LINE__);
	perror(NULL);

	return EXIT_FAILURE;
}
