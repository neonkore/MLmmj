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

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#include "find_email_adr.h"
#include "memory.h"

struct email_container *find_email_adr(const char *str,
		struct email_container *retstruct)
{
	size_t len;
	char *index_atsign;
	char *tempstr = mystrdup(str);
	char *c, *first_char = NULL, *last_char = NULL;
	
	index_atsign = strchr(tempstr, '@');
	while(index_atsign) {
		c = index_atsign;
		retstruct->emailcount++;
		while(c >= tempstr && *c != '<' && *c != ' ' && *c != ','
				&& *c != ';' && *c != '(' && *c != '[' &&
				*c >= 32 && *c != '{') {
			c--;
		}
		first_char = ++c;
		c = index_atsign;
		while(*c != '>' && *c != ' ' && *c != ',' && *c != ';'
				&& *c != ')' && *c != ']' && *c >= 32
				&& *c != '}' && *c != 0) {
			c++;
		}
		last_char = --c;

		len = last_char - first_char + 2;
		
		retstruct->emaillist = (char **)realloc(retstruct->emaillist,
				sizeof(char *) * retstruct->emailcount);
		retstruct->emaillist[retstruct->emailcount-1] =
				(char *)mymalloc(len + 1);
		snprintf(retstruct->emaillist[retstruct->emailcount-1], len,
			 "%s", first_char);
#if 0
		log_error(LOG_ARGS, "find_email_adr POST, [%s]\n",
				retstruct->emaillist[retstruct->emailcount-1]);
#endif
		*index_atsign = 'A'; /* Clear it so we don't find it again */
		index_atsign = strchr(tempstr, '@');
	}
	myfree(tempstr);
	return retstruct;
}
