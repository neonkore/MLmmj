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
#include "mygetline.h"
#include "strgen.h"

char *getlistaddr(const char *listdir)
{
	char *tmpstr;
	FILE *listnamefile;

	tmpstr = concatstr(2, listdir, "/listaddress");;
	if((listnamefile = fopen(tmpstr, "r")) == NULL) {
		log_error(LOG_ARGS, "Could not open '%s'", tmpstr);
		exit(EXIT_FAILURE);
	}
	free(tmpstr);

	tmpstr = myfgetline(listnamefile);

	if(!tmpstr){
		log_error(LOG_ARGS, "FATAL. Could not get listaddress");
		exit(EXIT_FAILURE);
	}

	chomp(tmpstr);
	fclose(listnamefile);

	return tmpstr;
}
