/* Copyright (C) 2005 Mads Martin Joergensen < mmj AT mmj DOT dk >
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
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "mlmmj.h"
#include "log_error.h"
#include "log_oper.h"
#include "strgen.h"
#include "mylocking.h"
#include "wrappers.h"
#include "memory.h"

int log_oper(const char *logfilename, const char *str)
{
	int fd;
	char ct[26], *logstr;
	struct stat st;
	time_t t;

	if(lstat(logfilename, &st) < 0 && errno != ENOENT) {
		log_error(LOG_ARGS, "Could not stat logfile %s", logfilename);
		return -1;
	} else if((st.st_mode & S_IFMT) == S_IFLNK) {
		log_error(LOG_ARGS, "%s is a symbolic link, not opening",
					logfilename);
		return -1;
	}
	
	fd = open(logfilename, O_RDWR|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if(fd < 0) {
		log_error(LOG_ARGS, "Could not open %s", logfilename);
		return -1;
	}

	if((time(&t) == (time_t)-1) || (ctime_r(&t, ct) == NULL))
		strncpy(ct, "Unknown time", sizeof(ct));

	if(myexcllock(fd) < 0) {
		log_error(LOG_ARGS, "Could not lock %s", logfilename);
		return -1;
	}

	logstr = concatstr(4, ct, ":", str, "\n");
	if(writen(fd, logstr, strlen(logstr)) < 0)
		log_error(LOG_ARGS, "Could not write to %s", logfilename);
	
	if(myunlock(fd) < 0)
		log_error(LOG_ARGS, "Could not unlock %s", logfilename);

	close(fd);
	myfree(logstr);
	
	return 0;
}
