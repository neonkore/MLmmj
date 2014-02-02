/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 * Copyright (C) 2006 Morten K. Poulsen <morten at afdelingp.dk>
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
#include "wrappers.h"
#include "log_error.h"
#include "chomp.h"


char *gethdrline(int fd, char **unfolded)
{
	char *line = NULL, *retstr = NULL, *oldretstr = NULL, *oldunfolded = NULL;
	char ch;
	ssize_t n;

	retstr = mygetline(fd);
	if (!retstr) {
		if (unfolded != NULL)
			*unfolded = NULL;
		return NULL;
	}

	if (unfolded != NULL)
		*unfolded = mystrdup(retstr);

	chomp(retstr);

	/* end-of-headers */
	if (*retstr == '\0') {
		if (unfolded != NULL) {
			myfree(*unfolded);
			*unfolded = NULL;
		}
		myfree(retstr);
		return NULL;
	}

	for(;;) {
		/* look ahead one char to determine if we need to fold */
		n = readn(fd, &ch, 1);
		if (n == 0) {  /* end of file, and therefore also headers */
			return retstr;
		} else if (n == -1) {  /* error */
			log_error(LOG_ARGS, "readn() failed in gethdrline()");
			if (unfolded != NULL) {
				myfree(*unfolded);
				*unfolded = NULL;
			}
			myfree(retstr);
			return NULL;
		}

		if (lseek(fd, -1, SEEK_CUR) == (off_t)-1) {
			log_error(LOG_ARGS, "lseek() failed in gethdrline()");
			if (unfolded != NULL) {
				myfree(*unfolded);
				*unfolded = NULL;
			}
			myfree(retstr);
			return NULL;
		}

		if ((ch != '\t') && (ch != ' '))  /* no more folding */
			return retstr;

		line = mygetline(fd);
		if (!line)
			return retstr;

		if (unfolded != NULL) {
			oldunfolded = *unfolded;
			*unfolded = concatstr(2, oldunfolded, line);
			myfree(oldunfolded);
		}

		chomp(line);
		oldretstr = retstr;
		retstr = concatstr(2, oldretstr, line);
		myfree(oldretstr);

		myfree(line);
	}
}
