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
#include "mlmmj-recieve.h"
#include "wrappers.h"
#include "strip_file_to_fd.h"
#include "header_token.h"
#include "getlistaddr.h"
#include "strgen.h"

void free_str_array(char **to_free)
{
	int i = 0;

	while(to_free[i])
		free(to_free[i++]);
	free(to_free);
}

static void print_help(const char *prg)
{
	        printf("Usage: %s -L /path/to/chat-list\n", prg);
		exit(EXIT_SUCCESS);
}


int main(int argc, char **argv)
{
	char *infilename;
	int mailfd;
	int opt;
	char *listdir = 0;
	char listadr[READ_BUFSIZE];
/*	char *scanbuf; */
	
	while ((opt = getopt(argc, argv, "hVL:")) != -1) {
		switch(opt) {
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
	if(listdir == 0) {
		fprintf(stderr, "You have to specify -L\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	/* get the list address */
	getlistaddr(listadr, listdir);
	
	infilename = concatstr(3, listdir, "/incoming/", random_str());
	mailfd = open(infilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	while(mailfd == -1 && errno == EEXIST) {
		free(infilename);
		infilename = concatstr(3, listdir, "/incoming/", random_str());
		mailfd = open(infilename, O_RDWR|O_CREAT|O_EXCL,
				S_IRUSR|S_IWUSR);
	}

	if(mailfd == -1) {
		free(infilename);
		perror(infilename);
		exit(EXIT_FAILURE);
	}

	printf("%s\n", infilename);
	
	/* strip_file_to_fd with 0, 0, 0, 0 as last arg just saves the file */
	strip_file_to_fd(stdin, mailfd, 0, 0, 0, 0);

	close(mailfd);
	close(mailfd);

	execlp(BINDIR"mlmmj-process", "mlmmj-process",
				"-L", listdir,
				"-m", infilename, 0);

	fprintf(stderr, "%s:%d execlp() of "BINDIR"mlmmj-send failed: ", __FILE__, __LINE__);
	perror(NULL);
	return EXIT_FAILURE;
}
