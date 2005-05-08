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
#include <string.h>
#include <unistd.h>

#include "mlmmj.h"
#include "mygetline.h"
#include "gethdrline.h"
#include "strgen.h"
#include "chomp.h"
#include "ctrlvalue.h"
#include "do_all_the_voodo_here.h"
#include "log_error.h"
#include "wrappers.h"
#include "memory.h"

int findit(const char *line, const char **headers)
{
	int i = 0;
	size_t len;

	while(headers[i]) {
		len = strlen(headers[i]);
		if(strncasecmp(line, headers[i], len) == 0)
			return 1;
		i++;
	}

	return 0;
}

void getinfo(const char *line, struct mailhdr *readhdrs)
{
	int i = 0;
	size_t tokenlen, linelen, valuelen;

	while(readhdrs[i].token) {
		tokenlen = strlen(readhdrs[i].token);
		linelen = strlen(line);
		if(strncasecmp(line, readhdrs[i].token, tokenlen) == 0) {
			readhdrs[i].valuecount++;
			valuelen = linelen - tokenlen + 1;
			readhdrs[i].values =
				(char **)myrealloc(readhdrs[i].values,
				  readhdrs[i].valuecount * sizeof(char *));
			readhdrs[i].values[readhdrs[i].valuecount - 1] =
					(char *)mymalloc(valuelen + 1);
			strncpy(readhdrs[i].values[readhdrs[i].valuecount - 1],
						line+tokenlen, valuelen);
			chomp(readhdrs[i].values[readhdrs[i].valuecount - 1]);
		}
		i++;
	}
}

int do_all_the_voodo_here(int infd, int outfd, int hdrfd, int footfd,
		 const char **delhdrs, struct mailhdr *readhdrs,
		 struct strlist *allhdrs, const char *prefix)
{
	char *hdrline, *subject, *unqp;
	int hdrsadded = 0;
	int subject_present = 0;

	allhdrs->count = 0;
	allhdrs->strs = NULL;

	while((hdrline = gethdrline(infd))) {
		/* Done with headers? Then add extra if wanted*/
		if((strncasecmp(hdrline, "mime", 4) == 0) ||
			((strlen(hdrline) == 1) && (hdrline[0] == '\n'))){

			/* add extra headers */
			if(!hdrsadded && hdrfd >= 0) {
				if(dumpfd2fd(hdrfd, outfd) < 0) {
					log_error(LOG_ARGS, "Could not "
						"add extra headers");
					myfree(hdrline);
					return -1;
				} else
					hdrsadded = 1;
			}
			
			fsync(outfd);

			/* end of headers, write single LF */ 
			if(hdrline[0] == '\n') {
				/* but first add Subject if none is present
				 * and a prefix is defined */
				if (prefix && !subject_present)
				{
					subject = concatstr(3, "Subject: ", 
								prefix, "\n");
					writen(outfd, subject, strlen(subject));
					myfree(subject);
					subject_present = 1;
				}

				if(writen(outfd, hdrline, strlen(hdrline))
						< 0) {
					myfree(hdrline);
					log_error(LOG_ARGS,
							"Error writing hdrs.");
					return -1;
				}
				myfree(hdrline);
				break;
			}
		}
		/* Do we want info from hdrs? Get it before it's gone */
		if(readhdrs)
			getinfo(hdrline, readhdrs);

		/* Snatch a copy of the header */
		allhdrs->count++;
		allhdrs->strs = myrealloc(allhdrs->strs,
					sizeof(char *) * (allhdrs->count + 1));
		allhdrs->strs[allhdrs->count-1] = mystrdup(hdrline);
		allhdrs->strs[allhdrs->count] = NULL;  /* XXX why, why, why? */

		/* Add Subject: prefix if wanted */
		if(prefix) {
			if(strncasecmp(hdrline, "Subject:", 8) == 0) {
				unqp = cleanquotedp(hdrline + 8);
				if(strstr(hdrline + 8, prefix) == NULL &&
				   strstr(unqp, prefix) == NULL) {
					subject = concatstr(3,
							"Subject: ", prefix,
							hdrline + 8);
					writen(outfd, subject,
							strlen(subject));
					myfree(subject);
					myfree(hdrline);
					myfree(unqp);
					subject_present = 1;
					continue;
				}
				myfree(unqp);
			}
		}
		
		/* Should it be stripped? */
		if(delhdrs) {
			if(!findit(hdrline, delhdrs))
				writen(outfd, hdrline, strlen(hdrline));
		} else
			writen(outfd, hdrline, strlen(hdrline));


		myfree(hdrline);
	}

	/* Just print the rest of the mail */
	if(dumpfd2fd(infd, outfd) < 0) {
		log_error(LOG_ARGS, "Error when dumping rest of mail");
		return -1;
	}

	/* No more, lets add the footer if one */
	if(footfd >= 0)
		if(dumpfd2fd(footfd, outfd) < 0) {
			log_error(LOG_ARGS, "Error when adding footer");
			return -1;
		}

	fsync(outfd);

	return 0;
}
