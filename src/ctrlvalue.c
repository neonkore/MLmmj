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
#include <stdio.h>

#include "strgen.h"
#include "ctrlvalue.h"
#include "mygetline.h"
#include "chomp.h"

char *ctrlvalue(const char *listdir, const char *ctrlstr)
{
	char *value = NULL;
	char *filename = concatstr(3, listdir, "/control/", ctrlstr);
	FILE *ctrlfile;

	ctrlfile = fopen(filename, "r");

	if(ctrlfile) {
		value = myfgetline(ctrlfile);
		chomp(value);
	}

	return value;
}

	
	
