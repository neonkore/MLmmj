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

#include "mlmmj.h"
#include "subscriberfuncs.h"
#include "mygetline.h"
#include "log_error.h"

int find_subscriber(int fd, const char *address)
{
	char *start, *cur, *next;
	struct stat st;
	size_t len;

	if(fstat(fd, &st) < 0) {
		log_error(LOG_ARGS, "Could not stat fd");
		return 1;
	}

	if((start = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0)) ==
			(void *)-1) {
		log_error(LOG_ARGS, "Could not mmap fd");
		return 1;
	}

	for(next = cur = start; next < start + st.st_size; next++) {
		if(*next == '\n') {
			len = next - cur;
			if((strlen(address) == len) &&
			   (strncasecmp(address, cur, len) == 0)) {
				munmap(start, st.st_size);
				return 0;
			}
			cur = next + 1;
		}
	}
	
	if(next > cur) {
		len = next - cur;
		if((strlen(address) == len) &&
		   (strncasecmp(address, cur, len) == 0)) {
			munmap(start, st.st_size);
			return 0;
		}
	}
	
	munmap(start, st.st_size);
	return 1;
}
