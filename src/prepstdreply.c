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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "prepstdreply.h"
#include "strgen.h"
#include "chomp.h"
#include "log_error.h"
#include "mygetline.h"
#include "wrappers.h"
#include "memory.h"

char *prepstdreply(const char *listdir, const char *filename, const char *from,
		   const char *to, const char *replyto, const char *subject,
		   size_t tokencount, char **data)
{
	int infd, outfd;
	size_t i;
	char *str, *tmp, *retstr = NULL;

	tmp = concatstr(3, listdir, "/text/", filename);
	infd = open(tmp, O_RDONLY);
	myfree(tmp);
	if(infd < 0) {
		log_error(LOG_ARGS, "Could not open std mail %s", filename);
		return NULL;
	}

	tmp = concatstr(6, "From: ", from,
			"\nTo: ", to,
			"\nSubject: ", subject);
	if(replyto)
		str = concatstr(3, tmp, "\nReply-To: ", replyto, "\n\n");
	else
		str = concatstr(2, tmp, "\n\n");
		
	myfree(tmp);

	do {
		tmp = random_str();
		myfree(retstr);
		retstr = concatstr(3, listdir, "/queue/", tmp);
		myfree(tmp);

		outfd = open(retstr, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);

	} while ((outfd < 0) && (errno == EEXIST));
	
	if(outfd < 0) {
		log_error(LOG_ARGS, "Could not open std mail %s", retstr);
		myfree(str);
		return NULL;
	}

	if(writen(outfd, str, strlen(str)) < 0) {
		log_error(LOG_ARGS, "Could not write std mail");
		myfree(str);
		return NULL;
	}
	myfree(str);

	while((str = mygetline(infd))) {
		for(i = 0; i < tokencount; i++) {
			if(strncmp(str, data[i*2], strlen(data[i*2])) == 0) {
				myfree(str);
				str = mystrdup(data[(i*2)+1]);
			}
		}
		if(writen(outfd, str, strlen(str)) < 0) {
			myfree(str);
			log_error(LOG_ARGS, "Could not write std mail");
			return NULL;
		}
		myfree(str);
	}
	
	fsync(outfd);
	close(outfd);

	return retstr;
}
