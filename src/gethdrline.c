/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdlib.h>
#include <unistd.h>

#include "mygetline.h"
#include "gethdrline.h"
#include "strgen.h"

char *gethdrline(int fd)
{
	char *line = NULL, *retstr = NULL, *nextline = NULL, *tmp = NULL;
	char ch;
	
	for(;;) {
		line = mygetline(fd);
		if(line == NULL)
			return NULL;
		if(read(fd, &ch, 1) == (size_t)1)
			lseek(fd, -1, SEEK_CUR);
		if(ch == '\t' || ch == ' ') {
			nextline = mygetline(fd);
			tmp = retstr;
			retstr = concatstr(3, retstr, line, nextline);
			free(tmp);
			free(line);
			free(nextline);
			tmp = line = nextline = NULL;
			if(read(fd, &ch, 1) == (size_t)1)
				lseek(fd, -1, SEEK_CUR);
			if(ch != '\t' && ch != ' ')
				return retstr;
		} else {
			tmp = retstr;
			retstr = concatstr(3, retstr, line, nextline);
			free(tmp);

			return retstr;
		}
	}
}
#if 0
#include <stdio.h>

int main(int argc, char **argv)
{
	char *str;

	while((str = gethdrline(fileno(stdin)))) {
		printf("[%s]", str);
		free(str);
	}

	free(str);

	return 0;
}
#endif
