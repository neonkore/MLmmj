/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj dot dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "wrappers.h"

#define DUMPBUF 4096

int dumpfd2fd(int infd, int outfd)
{
	size_t n;
	char buf[DUMPBUF];

	while((n = read(infd, &buf, sizeof(buf))) != 0) {
		if(n < 0) {
			if(errno == EINTR)
				continue;
			else
				return -1; /* Caller can check errno */
		}
		if(writen(outfd, &buf, n) < 0)
			return -1; /* Caller can check errno */
	}
	
	return 0;
}
