/* Copyright (C) 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "mlmmj.h"
#include "subscriberfuncs.h"
#include "getline.h"

int find_subscriber(int subfilefd, const char *address)
{
	char buf[READ_BUFSIZE];

	while (get_line(buf, sizeof(buf), subfilefd)) {
		while (buf[0] && isspace(buf[strlen(buf)-1]))
			buf[strlen(buf)-1] = '\0';
		if (strcasecmp(buf, address) == 0)
			return 0;
	}
	return 1;
}

