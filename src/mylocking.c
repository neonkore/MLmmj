/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <unistd.h>
#include <fcntl.h>
#include "mylocking.h"

int myexcllock(int fd)
{
	int mylock;
	struct flock locktype;

	locktype.l_type = F_WRLCK;
	locktype.l_whence = SEEK_SET;
	locktype.l_start = 0;
	locktype.l_len = 0;
	mylock = fcntl(fd, F_SETLKW, &locktype);

	return mylock;
}

int myunlock(int fd)
{
	int myunlock;
	struct flock locktype;

	locktype.l_type = F_UNLCK;
	myunlock = fcntl(fd, F_SETLKW, &locktype);

	return myunlock;
}
