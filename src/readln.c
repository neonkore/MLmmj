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
#include "readln.h"

/*! Read a line from filedescriptor fd. If bufsize-1 have been read and no
 * newline was found, zero terminate it and return. We can only guarantee to
 * have finished reading a line if there's a newline as the last char in the
 * buffer 'buf
 */
ssize_t readln(int fd, char *buf, size_t bufsize)
{
	ssize_t len = 0, rlen = 0;

	do {
		rlen = read(fd, buf + len, bufsize - len - 1);
		if(rlen < 0) {
			if(errno == EINTR)
				continue;
			else
				return -1;
		} else {
			if(rlen == 0) {
				buf[len] = '\0';
				return len;
			}
		}
		len += rlen;
	} while(buf[len - 1] != '\n');

	buf[len] = '\0';

	return len;
}
