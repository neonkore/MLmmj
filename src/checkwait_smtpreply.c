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

#define USEC_WAIT 1
#define LOOP_WAIT 10000

#define RFC_REPLY_SIZE 512

char *checkwait_smtpreply(int sockfd, int replytype)
{
	size_t len = 0;
	char smtpreply[RFC_REPLY_SIZE + 1];
	int timer = 0;

	smtpreply[RFC_REPLY_SIZE] = '\0';

	do {
		if(replytype != MLMMJ_QUIT && timer > 10) {
			usleep(USEC_WAIT);
			timer++;
		}
		len += read(sockfd, (smtpreply+len), RFC_REPLY_SIZE - len);
	} while(smtpreply[len-1] != '\n' && timer < LOOP_WAIT);

	smtpreply[len] = '\0';
#if 0
	printf("replytype = [%d], smtpreply = [%s]\n", replytype, smtpreply);
#endif
	if(timer > LOOP_WAIT) {
		printf("Timed out in waiting for reply--will try later\n");
		return (char *)-1;
	}
#if 0
	fprintf(stderr, "%s", smtpreply);
#endif

	/* This case might seem like (and is ATM) total overkill. But it's
	 * easy for us to extend it later on if needed.
	 */
	switch(replytype) {
	case MLMMJ_CONNECT:
		if(smtpreply[0] != '2' || smtpreply[1] != '2')
			return strdup(smtpreply);
		break;
	case MLMMJ_HELO:
		if(smtpreply[0] != '2' || smtpreply[1] != '5')
			return strdup(smtpreply);
		break;
	case MLMMJ_FROM:
		if(smtpreply[0] != '2' || smtpreply[1] != '5')
			return strdup(smtpreply);
		break;
	case MLMMJ_RCPTTO:
		if(smtpreply[0] != '2' || smtpreply[1] != '5')
			return strdup(smtpreply);
		break;
	case MLMMJ_DATA:
		if(smtpreply[0] != '3' || smtpreply[1] != '5')
			return strdup(smtpreply);
		break;
	case MLMMJ_DOT:
		if(smtpreply[0] != '2' || smtpreply[1] != '5')
			return strdup(smtpreply);
		break;
	case MLMMJ_QUIT:
		if(smtpreply[0] != '2' || smtpreply[1] != '2')
			return (char *)0xDEADBEEF;
		break;
	case MLMMJ_RSET:
		if(smtpreply[0] != '2' || smtpreply[1] != '5')
			return (char *)0xDEADBEEF;
		break;
	default:
		break;
	}

	return NULL;
}
