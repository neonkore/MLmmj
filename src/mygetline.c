/* Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
 * Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "mygetline.h"
#include "memory.h"

char *mygetline(int fd)
{
	size_t i = 0, res, buf_size = BUFSIZE;  /* initial buffer size */
	char *buf = mymalloc(buf_size);
	char ch;

	buf[0] = '\0';
	while(1) {	
		res = read(fd, &ch, 1);
		if(res < 0) {
			if(errno == EINTR)
				continue;
			else {
				myfree(buf);
				return NULL;
			}
		}
		if(res == 0) {
			if(buf[0]) {
				buf[i] = '\0';
				return buf;
			} else {
				myfree(buf);
				return NULL;
			}
		}

		if(i == buf_size - 1) {
			buf_size *= 2;
			buf = myrealloc(buf, buf_size);
		}
		buf[i++] = ch;
		if(ch == '\n') {
			buf[i] = '\0';
			return buf;
		}
	}
}
