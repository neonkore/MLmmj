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

#include "mlmmj.h"
#include "strgen.h"
#include "wrappers.h"
#include "log_error.h"
#include "subscriberfuncs.h"

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/list -a john=doe.org -n 12\n"
		" -a: Address string that bounces\n"
		" -h: This help\n"
		" -L: Full path to list directory\n"
		" -n: Message number in the archive\n"
		" -V: Print version\n", prg);

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int opt;
	char *listdir = NULL, *address = NULL, *number = NULL;
	char *filename, *bfilename, *a, *buf;
	size_t len;
	int fd;
	time_t t;
	off_t suboff;

	log_set_name(argv[0]);

	while ((opt = getopt(argc, argv, "hVL:a:n:")) != -1) {
		switch(opt) {
		case 'L':
			listdir = optarg;
			break;
		case 'a':
			address = optarg;
			break;
		case 'n':
			number = optarg;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'V':
			print_version(argv[0]);
			exit(0);
		}
	}
	if(listdir == NULL || address == NULL || number == NULL) {
		fprintf(stderr, "You have to specify -L, -a and -n\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

#if 0
	log_error(LOG_ARGS, "[%s] [%s] [%s]", listdir, address, number);
#endif
	/* save the filename with '=' before replacing it with '@' */
	bfilename = concatstr(3, listdir, "/bounce/", address);

	a = strchr(address, '=');
	if (!a) exit(EXIT_SUCCESS);  /* ignore malformed address */
	*a = '@';

	/* make sure it's a subscribed address */
	filename = concatstr(2, listdir, "/subscribers");
	if ((fd = open(filename, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", filename);
		free(filename); free(bfilename);
		exit(EXIT_FAILURE);
	}
	suboff = find_subscriber(fd, address);
	if(suboff == -1) {
		free(filename); free(bfilename);
		exit(EXIT_SUCCESS); /* Not subbed, so exit silently */
	}

	free(filename);

	/* TODO make sure the file we open below is not a symlink */
	if ((fd = open(bfilename, O_WRONLY|O_APPEND|O_CREAT,
			S_IRUSR|S_IWUSR)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", bfilename);
		free(filename); free(bfilename);
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
	snprintf(buf, len-26, "%s:%d # ", number, (int)t);
	ctime_r(&t, buf+strlen(buf));
	writen(fd, buf, strlen(buf));
	close(fd);

	return EXIT_FAILURE;
}
