/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <string.h>
#include "chomp.h"

char *chomp(char *str)
{
	int i = strlen(str) - 1;

	while(str[i] == '\n') {
		str[i] = 0;
		i--;
	}

	return str;
}
