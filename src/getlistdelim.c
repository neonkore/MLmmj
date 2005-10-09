/* Copyright 2005 Joel Aelwyn <joel@lightbearer.com>
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS+ * IN THE SOFTWARE.
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mlmmj.h"
#include "getlistdelim.h"
#include "chomp.h"
#include "log_error.h"
#include "mygetline.h"
#include "strgen.h"
#include "memory.h"

char *getlistdelim(const char *listdir)
{
	char *delimfn, *delimstr;
	int delimfd;

	delimfn = concatstr(2, listdir, "/control/delimiter");
	if(-1 != (delimfd = open(delimfn, O_RDONLY))){
		delimstr = mygetline(delimfd);
		close(delimfd);

		if(NULL == delimstr){
			log_error(LOG_ARGS,
				  "FATAL. Could not get list delimiter from %s",
				  delimfn);
			myfree(delimfn);
			exit(EXIT_FAILURE);
		}
	
		chomp(delimstr);
	
		if(0 == strlen(delimstr)){
			log_error(LOG_ARGS,
				  "FATAL. Zero-length delimiter found from %s",
				  delimfn);
			myfree(delimfn);
			exit(EXIT_FAILURE);
		}
	} else
		delimstr = mystrdup(DEFAULT_RECIPDELIM);

	myfree(delimfn);

	return delimstr;
}
