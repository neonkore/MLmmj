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
#include "find_email_adr.h"
#include "incindexfile.h"
#include "getlistaddr.h"
#include "listcontrol.h"
#include "strgen.h"
#include "do_all_the_voodo_here.h"

static void print_help(const char *prg)
{
	        printf("Usage: %s [-P] -L /path/to/chat-list\n"
		       "          -m mailfile\n", prg);
		exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int fd, opt, i, noprocess = 0;
	char *listdir = NULL, *mailfile = NULL, *headerfilename = NULL;
	char *footerfilename = NULL, *donemailname = NULL;
	char *randomstr = random_str();
	FILE *headerfile, *footerfile, *rawmailfile, *donemailfile;
	struct email_container toemails = { 0, NULL };
	const char *badheaders[] = { "From ", "Return-Path:", NULL };
	struct mailhdr readhdrs[] = {
		{ "To:", NULL },
		{ "Cc:", NULL },
		{ NULL, NULL }
	};
	
	while ((opt = getopt(argc, argv, "hVPm:L:")) != -1) {
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
		case 'P':
			noprocess = 1;
			break;
		case 'V':
			print_version(argv[0]);
			exit(0);
		}
	}
	if(listdir == NULL || mailfile == NULL) {
		fprintf(stderr, "You have to specify -L and -m\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	donemailname = concatstr(3, listdir, "/queue/", randomstr);
	free(randomstr);
	fd = open(donemailname, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	while(fd == -1 && errno == EEXIST) {
		free(donemailname);
		randomstr = random_str();
		donemailname = concatstr(3, listdir, "/queue/", randomstr);
		free(randomstr);
		fd = open(donemailname, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	}
	
	if(fd == -1) {
		free(donemailname);
		fprintf(stderr, "%s:%d could not get fd in %s: ",
                                __FILE__, __LINE__, donemailname);
		exit(EXIT_FAILURE);
	}

	if((donemailfile = fdopen(fd, "w")) == NULL) {
		free(donemailname);
		fprintf(stderr, "%s:%d could not fdopen",
				__FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	printf("[%s]\n", donemailname);

	if((rawmailfile = fopen(mailfile, "r")) == NULL) {
		free(donemailname);
		fprintf(stderr, "%s:%d could not open %s: ",
                                __FILE__, __LINE__, mailfile);
		exit(EXIT_FAILURE);
	}

	headerfilename = concatstr(2, listdir, "/customheaders");
	headerfile = fopen(headerfilename, "r");
	free(headerfilename);
	
	footerfilename = concatstr(2, listdir, "/footer");
	footerfile = fopen(footerfilename, "r");
	free(footerfilename);
	
	do_all_the_voodo_here(rawmailfile, donemailfile, headerfile,
				footerfile, badheaders, readhdrs);

	fclose(rawmailfile);
	/*XXX: unlink(mailfile);*/
	close(fd);
	fclose(donemailfile);

	if(headerfile)
		fclose(headerfile);
	if(footerfile)
		fclose(footerfile);

	if(readhdrs[0].value) {
		find_email_adr(readhdrs[0].value, &toemails);
#if 0
		for(i = 0; i < toemails.emailcount; i++)
			printf("toemails.emaillist[%d] = %s\n", i,
					toemails.emaillist[i]);
	}
	if(readhdrs[1].value) {
		find_email_adr(readhdrs[1].value, &ccemails);
		for(i = 0; i < ccemails.emailcount; i++)
			printf("ccemails.emaillist[%d] = %s\n", i,
					ccemails.emaillist[i]);
#endif
	}

	if(strchr(toemails.emaillist[0], RECIPDELIM)) {
		printf("listcontrol(%s, %s, %s)\n", donemailname, listdir,
						toemails.emaillist[0]);
		listcontrol(donemailname, listdir, toemails.emaillist[0]);
		return EXIT_SUCCESS;
	}

	if(noprocess) {
		free(donemailname);
		/* XXX: toemails and ccemails etc. have to be free() */
		exit(EXIT_SUCCESS);
	}

	execlp(BINDIR"mlmmj-send", "mlmmj-send",
				"-L", listdir,
				"-m", donemailname, 0);
	fprintf(stderr, "%s:%d execlp() of mlmmj-send failed: ",
			__FILE__, __LINE__);
	perror(NULL);
	return EXIT_FAILURE;
}
