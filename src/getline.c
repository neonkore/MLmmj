/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "getline.h"

char *get_line(char *buf, int count, int infd)
{
	int bufcount = 0;
	size_t c;
	char ch[1];
	
	ch[0] = 0;
	while((c = read(infd, ch, 1))) {
		if(ch[0] != '\n' && bufcount < count)
			buf[bufcount++] = ch[0];
		else {
			buf[bufcount] = '\n';
			if(bufcount < count - 1)
				buf[bufcount + 1] = 0;
			return buf;
		}
	}
	if(ch[0]) {
		if(bufcount < count - 1)
			buf[bufcount + 1] = 0;
		return buf;
	} else
		return 0;
}
