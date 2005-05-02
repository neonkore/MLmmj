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
#include "memory.h"

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
		myfree(indexfilename);
		log_error(LOG_ARGS, "Error opening index file");
		return 0;
	}

	lock = myexcllock(fd);
	
	if(lock) {
		myfree(indexfilename);
		log_error(LOG_ARGS, "Error locking index file");
		close(fd);
		return 0;
	}

	readn(fd, intbuf, INTBUF_SIZE);
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

	close(fd); /* Lock is also released */
	myfree(indexfilename);

	return index;
}
