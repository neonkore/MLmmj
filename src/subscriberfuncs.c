/* Copyright (C) 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
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

off_t find_subscriber(int fd, const char *address)
{
	char *start, *cur, *next;
	struct stat st;
	size_t len;

	if(fstat(fd, &st) < 0) {
		log_error(LOG_ARGS, "Could not stat fd");
		return (off_t)-1;
	}

	if(!S_ISREG(st.st_mode)) {
		log_error(LOG_ARGS, "Non regular file in subscribers.d/");
		return (off_t)-1;
	}

	if((start = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) ==
			(void *)-1) {
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

int is_subbed(const char *listdir, const char *address)
{
	int retval = 1, subread;
	char *subddirname, *subreadname;
	off_t suboff;
	DIR *subddir;
	struct dirent *dp;

	subddirname = concatstr(2, listdir, "/subscribers.d/");
	if((subddir = opendir(subddirname)) == NULL) {
		log_error(LOG_ARGS, "Could not opendir(%s)", subddirname);
		free(subddirname);
		exit(EXIT_FAILURE);
	}

	free(subddirname);

	while((dp = readdir(subddir)) != NULL) {
		if(!strcmp(dp->d_name, "."))
			continue;
		if(!strcmp(dp->d_name, ".."))
			continue;

		subreadname = concatstr(3, listdir, "/subscribers.d/",
				dp->d_name);
		subread = open(subreadname, O_RDONLY);
		if(subread < 0) {
			log_error(LOG_ARGS, "Could not open '%s'",
					subreadname);
			free(subreadname);
			continue;
		}

		suboff = find_subscriber(subread, address);
		close(subread);
		free(subreadname);

		if(suboff == -1) {
			continue;
		} else {
			retval = 0;
			break;
		}
	}
	closedir(subddir);

	return retval;
}
