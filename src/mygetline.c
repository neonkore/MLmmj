/* Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
 * Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "mygetline.h"

char *mygetline(int fd)
{
	size_t i = 0, res, buf_size = BUFSIZE;  /* initial buffer size */
	char *buf = malloc(buf_size);
	char ch;

	buf[0] = '\0';
	while(1) {	
		res = read(fd, &ch, 1);
		if(res < 0) {
			if(errno == EINTR)
				continue;
			else {
				free(buf);
				return NULL;
			}
		}
		if(res == 0) {
			if(buf[0]) {
				buf[i] = '\0';
				return buf;
			} else {
				free(buf);
				return NULL;
			}
		}

		if(i == buf_size - 1) {
			buf_size *= 2;
			buf = realloc(buf, buf_size);
		}
		buf[i++] = ch;
		if(ch == '\n') {
			buf[i] = '\0';
			return buf;
		}
	}
}

#if 0
char *myfgetline(FILE *infile)
{
	size_t buf_size = BUFSIZE;  /* initial buffer size */
	size_t buf_used;
	char *buf = malloc(buf_size);
	
	buf[0] = '\0';
	
	if(infile == NULL)
		return NULL;

	for (;;) {
		buf_used = strlen(buf);
		if (fgets(buf+buf_used, buf_size-buf_used, infile) == NULL) {
			if (buf[0]) {
				 return buf;
			} else {
				free(buf);
				return NULL;
			}
		}

		if ((strlen(buf) < buf_size-1) || (buf[buf_size-1] == '\n')) {
			return buf;
		}

		/* grow buffer */
		buf_size *= 2;
		buf = realloc(buf, buf_size);

	}
}

int main(int argc, char **argv)
{
	char *str;
	
	while((str = mygetline(fileno(stdin)))) {
		printf("%s", str);
		free(str);
	}

	return 0;
}
#endif
