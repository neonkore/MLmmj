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
#include <fcntl.h>
#include <unistd.h>

#include "strgen.h"
#include "ctrlvalue.h"
#include "mygetline.h"
#include "chomp.h"

char *ctrlvalue(const char *listdir, const char *ctrlstr)
{
	char *value = NULL;
	char *filename = concatstr(3, listdir, "/control/", ctrlstr);
	int ctrlfd;

	ctrlfd = open(filename, O_RDONLY);
	free(filename);

	if(ctrlfd < 0)
		return NULL;
		
	value = mygetline(ctrlfd);
	close(ctrlfd);
	chomp(value);

	return value;
}

	
	
