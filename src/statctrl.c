/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "strgen.h"
#include "statctrl.h"

int statctrl(const char *listdir, const char *ctrlstr)
{
	char *filename = concatstr(3, listdir, "/control/", ctrlstr);
	struct stat st;
	int res;
	
	res = stat(filename, &st);
	free(filename);
	
	if(res == 0)
		return 1;

	return 0;
}
