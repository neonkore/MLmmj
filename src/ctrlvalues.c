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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "strgen.h"
#include "ctrlvalues.h"
#include "mygetline.h"
#include "chomp.h"
#include "mlmmj.h"
#include "memory.h"

struct strlist *ctrlvalues(const char *listdir, const char *ctrlstr)
{
	struct strlist *ret;
	char *filename = concatstr(3, listdir, "/control/", ctrlstr);
	char *value;
	int ctrlfd;

	ctrlfd = open(filename, O_RDONLY);
	myfree(filename);

	if(ctrlfd < 0)
		return NULL;
		
	ret = mymalloc(sizeof(struct strlist));
	ret->count = 0;
	ret->strs = NULL;
	while((value = mygetline(ctrlfd)) != NULL) {
		chomp(value);
		/* Ignore empty lines */
		if (*value == '\0')
			continue;
		ret->count++;
		ret->strs = (char **) myrealloc(ret->strs, sizeof(char *) *
					(ret->count + 1));
		ret->strs[ret->count-1] = value;
		ret->strs[ret->count] = NULL;
	}
		
	close(ctrlfd);

	return ret;
}
