/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

/**
 * write n bytes to a descriptor 
 */

ssize_t writen(int fd, const void *vptr, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = (const char*)vptr;
	nleft = n;
	while(nleft > 0) {
		if((nwritten = write(fd, ptr, nleft)) < 0) {
			if(errno == EINTR) {
				nwritten = 0; /* and call write() again */
			} else {
				return -1; /* error, caller can check errno */
			}
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return n;
}
