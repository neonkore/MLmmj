/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <unistd.h>

#include "mygetline.h"
#include "gethdrline.h"
#include "strgen.h"
#include "memory.h"

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
			myfree(tmp);
			myfree(line);
			myfree(nextline);
			tmp = line = nextline = NULL;
			if(read(fd, &ch, 1) == (size_t)1)
				lseek(fd, -1, SEEK_CUR);
			if(ch != '\t' && ch != ' ')
				return retstr;
		} else {
			tmp = retstr;
			retstr = concatstr(3, retstr, line, nextline);
			myfree(line);
			myfree(tmp);

			return retstr;
		}
	}
}
