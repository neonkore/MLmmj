/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <fcntl.h>
#include <unistd.h>
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
	int listnamefd;

	tmpstr = concatstr(2, listdir, "/listaddress");;
	if((listnamefd = open(tmpstr, O_RDONLY)) < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", tmpstr);
		exit(EXIT_FAILURE);
	}
	free(tmpstr);

	tmpstr = mygetline(listnamefd);

	if(tmpstr == NULL){
		log_error(LOG_ARGS, "FATAL. Could not get listaddress in %s",
					listdir);
		exit(EXIT_FAILURE);
	}

	chomp(tmpstr);
	close(listnamefd);

	return tmpstr;
}
