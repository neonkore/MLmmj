/* Copyright (C) 2003, 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "mlmmj.h"

void print_version(const char *prg)
{
	char *lastslashindex = strrchr(prg, '/');
	
	if(!lastslashindex)
		lastslashindex = (char *)prg;
	else
		lastslashindex++;

	printf("%s version "VERSION"\n", lastslashindex);	
}
