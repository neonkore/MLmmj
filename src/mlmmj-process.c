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

#include "mlmmj-process.h"
#include "mlmmj.h"
#include "wrappers.h"
#include "strip_file_to_fd.h"
#include "header_token.h"
#include "find_email_adr.h"
#include "incindexfile.h"
#include "getlistaddr.h"
#include "listcontrol.h"
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
	        printf("Usage: %s -L /path/to/chat-list\n"
		       "          -m mailfile\n", prg);
		exit(EXIT_SUCCESS);
}


int main(int argc, char **argv)
{
	char *donemailname = 0;
	const char *badheaders[] = {"From ", "Return-Path:", 0};
	int donemailfd, opt;
	char *listdir = 0;
	char listadr[READ_BUFSIZE];
	char tovalue[READ_BUFSIZE];
	char *mailfile = 0;
	char *headerfilename = 0;
	char *footerfilename = 0;
	FILE *headerfile, *footerfile, *rawmailfile;
	struct email_container toemails;
	
	while ((opt = getopt(argc, argv, "hVm:L:")) != -1) {
		switch(opt) {
		case 'L':
			listdir = optarg;
			break;
		case 'm':
			mailfile = optarg;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'V':
			print_version(argv[0]);
			exit(0);
		}
	}
	if(listdir == 0 || mailfile == 0) {
		fprintf(stderr, "You have to specify -L and -m\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	/* get the list address */
	getlistaddr(listadr, listdir);

	donemailname = concatstr(3, listdir, "/queue/", random_str());
	donemailfd = open(donemailname, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	while(donemailfd == -1 && errno == EEXIST) {
		donemailname = concatstr(3, listdir, "/queue/", random_str());
		donemailfd = open(donemailname, O_RDWR|O_CREAT|O_EXCL,
				  S_IRUSR|S_IWUSR);
	}
	
	if(donemailfd == -1) {
		free(donemailname);
		perror("Cannot open queuemailfile");
		exit(EXIT_FAILURE);
	}

	printf("%s\n", donemailname);

	if((rawmailfile = fopen(mailfile, "r")) == NULL) {
		perror("Cannot open mailfile");
		exit(EXIT_FAILURE);
	}

	headerfilename = concatstr(2, listdir, "/customheaders");
	headerfile = fopen(headerfilename, "r");
	free(headerfilename);
	
	footerfilename = concatstr(2, listdir, "/footer");
	footerfile = fopen(footerfilename, "r");
	free(footerfilename);
	
	strip_file_to_fd(rawmailfile, donemailfd, badheaders, headerfile,
			 footerfile, tovalue);
	close(donemailfd);

	fclose(rawmailfile);
	unlink(mailfile);

	if(tovalue) {
		find_email_adr(tovalue, &toemails);
		if(index(toemails.emaillist[0], RECIPDELIM))
			listcontrol(donemailname, listdir, toemails.emaillist[0]);
	}
	close(donemailfd);
	if(headerfile)
		fclose(headerfile);
	
	execlp(BINDIR"mlmmj-send", "mlmmj-send",
				"-L", listdir,
				"-m", donemailname, 0);
	fprintf(stderr, "%s:%d execlp() of "BINDIR"mlmmj-send failed: ", __FILE__, __LINE__);
	perror(NULL);
	return EXIT_FAILURE;
}
