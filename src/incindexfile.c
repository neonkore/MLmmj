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
#include "strgen.h"

#define INTBUF_SIZE 32

int incindexfile(const char *listdir)
{
	int fd, lock;
	long int index = 0;
	char intbuf[INTBUF_SIZE] = "uninitialized";
	size_t i;
	char *indexfilename;

	indexfilename = concatstr(2, listdir, "/index");
	fd = open(indexfilename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);

	if(fd == -1) {
		free(indexfilename);
		log_error(LOG_ARGS, "Error opening index file");
		return 0;
	}

	lock = myexcllock(fd);
	
	if(lock) {
		free(indexfilename);
		log_error(LOG_ARGS, "Error locking index file");
		close(fd);
		return 0;
	}

	read(fd, intbuf, INTBUF_SIZE);
	for(i=0; i<sizeof(intbuf); i++) {
		if(intbuf[i] < '0' || intbuf[i] > '9') {
			intbuf[i] = '\0';
			break;
		}
	}
	index = atol(intbuf);
	index++;
	itoa(index, intbuf);
	lseek(fd, 0, SEEK_SET);
	writen(fd, intbuf, strlen(intbuf));

	myunlock(fd);
	close(fd);
	free(indexfilename);

	return index;
}
