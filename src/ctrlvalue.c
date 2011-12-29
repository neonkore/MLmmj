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
#include <string.h>

#include "strgen.h"
#include "ctrlvalue.h"
#include "mygetline.h"
#include "chomp.h"
#include "memory.h"

static char *ctrlval(const char *listdir, const char *subdir,
		const char *ctrlstr, int oneline)
{
	char *filename, *value = NULL;
	int ctrlfd, i;

	if(listdir == NULL)
		return NULL;

	filename = concatstr(5, listdir, "/", subdir, "/", ctrlstr);
	ctrlfd = open(filename, O_RDONLY);
	myfree(filename);

	if(ctrlfd < 0)
		return NULL;

	if (oneline) {
		value = mygetline(ctrlfd);
		chomp(value);
	} else {
		value = mygetcontent(ctrlfd);
		i = strlen(value) - 1;
		if (i >= 0 && value[i] == '\n') {
			value[i] = '\0';
			i--;
		}
		if (i >= 0 && value[i] == '\r') {
			value[i] = '\0';
			i--;
		}
	}
	close(ctrlfd);

	return value;
}

char *ctrlvalue(const char *listdir, const char *ctrlstr)
{
	return ctrlval(listdir, "control", ctrlstr, 1);
}

char *ctrlcontent(const char *listdir, const char *ctrlstr)
{
	return ctrlval(listdir, "control", ctrlstr, 0);
}

char *textcontent(const char *listdir, const char *ctrlstr)
{
	return ctrlval(listdir, "text", ctrlstr, 0);
}

