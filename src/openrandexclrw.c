/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj dot dk>,
 *                    Morten K. Poulsen <morten at afdelingp dot dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "strgen.h"
#include "wrappers.h"

int openrandexclrw(const char *dir, const char *prefix, mode_t mode)
{
	int fd;
	char *filename = NULL, *randomstr;

	do {
                randomstr = random_str();
		free(filename);  /* It is OK to free() NULL, as this
				    will in the first iteration. */
		filename = concatstr(4, dir, "/", prefix, randomstr);
		free(randomstr);

                fd = open(filename, O_RDWR|O_CREAT|O_EXCL, mode);

	} while ((fd < 0) && (errno == EEXIST));

	return fd;
}
