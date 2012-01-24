/* Copyright (C) 2003 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include "mlmmj.h"
#include "subscriberfuncs.h"
#include "mygetline.h"
#include "log_error.h"
#include "wrappers.h"
#include "strgen.h"
#include "memory.h"

char *subtype_strs[] = {
	"normal",
	"digest",
	"nomail",
	"file",
	"all",
	"none"
};

char * subreason_strs[] = {
	"request",
	"confirm",
	"permit",
	"admin",
	"bouncing",
	"switch"
};

off_t find_subscriber(int fd, const char *address)
{
	char *start, *cur, *next;
	struct stat st;
	size_t len;

	if(fstat(fd, &st) < 0) {
		log_error(LOG_ARGS, "Could not stat fd");
		return (off_t)-1;
	}

	/* No need to check in 0-size file */
	if(st.st_size == 0)
		return (off_t)-1;

	if(!S_ISREG(st.st_mode)) {
		log_error(LOG_ARGS, "Non regular file in subscribers.d/");
		return (off_t)-1;
	}

	if((start = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) ==
			MAP_FAILED) {
		log_error(LOG_ARGS, "Could not mmap fd");
		return (off_t)-1;
	}
	
	for(next = cur = start; next < start + st.st_size; next++) {
		if(*next == '\n') {
			len = next - cur;
			if((strlen(address) == len) &&
			   (strncasecmp(address, cur, len) == 0)) {
				munmap(start, st.st_size);
				return (off_t)(cur - start);
			}
			cur = next + 1;
		}
	}
	
	if(next > cur) {
		len = next - cur;
		if((strlen(address) == len) &&
		   (strncasecmp(address, cur, len) == 0)) {
			munmap(start, st.st_size);
			return (off_t)(cur - start);
		}
	}
	
	munmap(start, st.st_size);
	return (off_t)-1;
}

int is_subbed_in(const char *subddirname, const char *address)
{
	int retval = 0, subread;
	char *subreadname;
	off_t suboff;
	DIR *subddir;
	struct dirent *dp;

	if((subddir = opendir(subddirname)) == NULL) {
		log_error(LOG_ARGS, "Could not opendir(%s)", subddirname);
		exit(EXIT_FAILURE);
	}

	while((dp = readdir(subddir)) != NULL) {
		if(!strcmp(dp->d_name, "."))
			continue;
		if(!strcmp(dp->d_name, ".."))
			continue;

		subreadname = concatstr(2, subddirname, dp->d_name);
		subread = open(subreadname, O_RDONLY);
		if(subread < 0) {
			myfree(subreadname);
			continue;
		}

		suboff = find_subscriber(subread, address);
		close(subread);
		myfree(subreadname);

		if(suboff == -1) {
			continue;
		} else {
			retval = 1;
			break;
		}
	}
	closedir(subddir);

	return retval;
}

enum subtype is_subbed(const char *listdir, const char *address)
{
	int retval;
	char *subddirname;
	
	subddirname = concatstr(2, listdir, "/subscribers.d/");
	retval = is_subbed_in(subddirname, address);
	myfree(subddirname);
	if (retval) return SUB_NORMAL;

	subddirname = concatstr(2, listdir, "/digesters.d/");
	retval = is_subbed_in(subddirname, address);
	myfree(subddirname);
	if (retval) return SUB_DIGEST;

	subddirname = concatstr(2, listdir, "/nomailsubs.d/");
	retval = is_subbed_in(subddirname, address);
	myfree(subddirname);
	if (retval) return SUB_NOMAIL;

	return SUB_NONE;
}
