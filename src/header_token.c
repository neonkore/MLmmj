/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "mlmmj.h"
#include "header_token.h"
#include "wrappers.h"

char *find_header(char *buf, const char *token)
{
	if(strncmp(buf, token, strlen(token)) == 0)
		return &(buf[strlen(token)+1]);
	else
		return 0;
}

char *find_header_file(FILE *in, char *scanbuf, const char *header_token)
{
	char *bufres;

	while((bufres=fgets(scanbuf, READ_BUFSIZE, in)) != NULL)
		if(scanbuf[0] != '\t')
			if((bufres = find_header(scanbuf, header_token)))
				return bufres;
	return 0;
}

