/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "wrappers.h"
#include "mylocking.h"
#include "incindexfile.h"
#include "itoa.h"
#include "log_error.h"

#define INTBUF_SIZE 32

int incindexfile(const char *listdir, int incflag)
{
	size_t len;
	int fd, lock;
	long int mindex = 0, oldindex = 0;
	char intbuf[INTBUF_SIZE] = "uninitialized";
	int s;
	char *indexfilename;

	len = strlen(listdir) + strlen("/index") + 1;
	indexfilename = malloc(len);
	snprintf(indexfilename, len, "%s%s", listdir, "/index");
#ifdef MLMMJ_DEBUG
	fprintf(stderr, "indexfilename = [%s]\n", indexfilename);
#endif
	fd = open(indexfilename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);

	if(fd == -1) {
		log_error(LOG_ARGS, "Error opening index file");
		exit(EXIT_FAILURE);
	}

	lock = myexcllock(fd);
	
	if(lock) {
		log_error(LOG_ARGS, "Error locking index file");
		close(fd);
		exit(EXIT_FAILURE);
	}

	read(fd, intbuf, INTBUF_SIZE);
	for(s = INTBUF_SIZE - 1; s >= 0; s--)
		if(intbuf[s] < '0' || intbuf[s] > '9')
			intbuf[s] = 0;
	oldindex = atol(intbuf);
	if(incflag) {
		mindex = oldindex;
		mindex++;
		itoa(mindex, intbuf);
		lseek(fd, 0, SEEK_SET);
		writen(fd, intbuf, strlen(intbuf));
	}
	myunlock(fd);
	close(fd);
	free(indexfilename);

	return oldindex;
}	
