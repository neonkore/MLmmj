/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "getlistaddr.h"
#include "chomp.h"
#include "log_error.h"

#define MAXLISTNAMELEN 1024

char *getlistaddr(char *listaddrdeststr, const char *listdir)
{
	size_t len;
	char *tmpstr;
	FILE *listnamefile;

	len = strlen(listdir) + strlen("/listaddress") + 1;
	tmpstr = malloc(len);

	snprintf(tmpstr, len, "%s/listaddress", listdir);
	if((listnamefile = fopen(tmpstr, "r")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", tmpstr);
		exit(EXIT_FAILURE);
	}

	fgets(listaddrdeststr, MAXLISTNAMELEN, listnamefile);
	chomp(listaddrdeststr);

	fclose(listnamefile);
	free(tmpstr);
	return listaddrdeststr;
}
