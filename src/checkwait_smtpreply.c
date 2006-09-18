/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "checkwait_smtpreply.h"
#include "config.h"
#include "memory.h"
#include "mygetline.h"


char *checkwait_smtpreply(int sockfd, int replytype)
{
	char *smtpreply;

	smtpreply = mygetline(sockfd);
#if 0
	printf("replytype = [%d], smtpreply = [%s]\n", replytype, smtpreply);
	fprintf(stderr, "%s", smtpreply);
#endif

	/* This case might seem like (and is ATM) total overkill. But it's
	 * easy for us to extend it later on if needed.
	 */
	switch(replytype) {
		case MLMMJ_CONNECT:
			if(smtpreply[0] != '2' || smtpreply[1] != '2')
				return mystrdup(smtpreply);
			break;
		case MLMMJ_HELO:
			if(smtpreply[0] != '2' || smtpreply[1] != '5')
				return mystrdup(smtpreply);
			break;
		case MLMMJ_FROM:
			if(smtpreply[0] != '2' || smtpreply[1] != '5')
				return mystrdup(smtpreply);
			break;
		case MLMMJ_RCPTTO:
			if(smtpreply[0] != '2' || smtpreply[1] != '5')
				return mystrdup(smtpreply);
			break;
		case MLMMJ_DATA:
			if(smtpreply[0] != '3' || smtpreply[1] != '5')
				return mystrdup(smtpreply);
			break;
		case MLMMJ_DOT:
			if(smtpreply[0] != '2' || smtpreply[1] != '5')
				return mystrdup(smtpreply);
			break;
		case MLMMJ_QUIT:
			if(smtpreply[0] != '2' || smtpreply[1] != '2')
				return mystrdup(smtpreply);
			break;
		case MLMMJ_RSET:
			if(smtpreply[0] != '2' || smtpreply[1] != '5')
				return mystrdup(smtpreply);
			break;
		default:
			break;
	}

	return NULL;
}
