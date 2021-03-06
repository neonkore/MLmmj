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
#include "ctrlvalue.h"
#include "do_all_the_voodoo_here.h"
#include "log_error.h"
#include "wrappers.h"
#include "memory.h"

int findit(const char *line, const struct strlist *headers)
{
	int i;
	size_t len;

	for (i=0;i<headers->count;i++) {
		len = strlen(headers->strs[i]);
		if(strncasecmp(line, headers->strs[i], len) == 0)
			return 1;
	}

	return 0;
}

void getinfo(const char *line, struct mailhdr *readhdrs)
{
	int i = 0;
	size_t tokenlen, valuelen;

	while(readhdrs[i].token) {
		tokenlen = strlen(readhdrs[i].token);
		if(strncasecmp(line, readhdrs[i].token, tokenlen) == 0) {
			readhdrs[i].valuecount++;
			valuelen = strlen(line) - tokenlen;
			readhdrs[i].values =
				(char **)myrealloc(readhdrs[i].values,
				  readhdrs[i].valuecount * sizeof(char *));
			readhdrs[i].values[readhdrs[i].valuecount - 1] =
					(char *)mymalloc(valuelen + 1);
			strcpy(readhdrs[i].values[readhdrs[i].valuecount - 1],
						line+tokenlen);
		}
		i++;
	}
}

int do_all_the_voodoo_here(int infd, int outfd, int hdrfd, int footfd,
		 const struct strlist *delhdrs, struct mailhdr *readhdrs,
		 struct strlist *allhdrs, const char *prefix, const char *listaddr)
{
	char *hdrline, *unfolded, *subject, *unqp, *from, *replyto = NULL;
	int hdrsadded = 0;
	int subject_present = 0;
	int replyto_present = 0;

	allhdrs->count = 0;
	allhdrs->strs = NULL;

	for(;;) {
		hdrline = gethdrline(infd, &unfolded);

		/* add extra headers before MIME* headers,
		   or after all headers */
		if(!hdrsadded &&
				(hdrline == NULL ||
				 strncasecmp(hdrline, "mime", 4) == 0)) {
			if(hdrfd >= 0) {
				if(dumpfd2fd(hdrfd, outfd) < 0) {
					log_error(LOG_ARGS, "Could not "
						"add extra headers");
					myfree(hdrline);
					myfree(unfolded);
					return -1;
				}
				fsync(outfd);
			}
			hdrsadded = 1;
		}

		/* end of headers */ 
		if(hdrline == NULL) {
			/* add Subject if none is present
			   and a prefix is defined */
			if (prefix && !subject_present) {
				subject = concatstr(3,"Subject: ",prefix,"\n");
				writen(outfd, subject, strlen(subject));
				myfree(subject);
				subject_present = 1;
			}
			/* always have a Reply-to: if munging From: */
			if ( replyto != NULL ) {
			    if ( (listaddr != NULL) && (replyto_present == 0) ) {
				writen(outfd, replyto, strlen(replyto));
			    }
			    myfree(replyto);
			}
			/* write LF */
			if(writen(outfd, "\n", 1) < 0) {
				myfree(hdrline);
				myfree(unfolded);
				log_error(LOG_ARGS, "Error writing hdrs.");
				return -1;
			}
			myfree(hdrline);
			myfree(unfolded);
			break;
		}

		/* Do we want info from hdrs? Get it before it's gone */
		if(readhdrs)
			getinfo(hdrline, readhdrs);

		/* Snatch a copy of the header */
		allhdrs->count++;
		allhdrs->strs = myrealloc(allhdrs->strs,
					sizeof(char *) * (allhdrs->count));
		allhdrs->strs[allhdrs->count-1] = mystrdup(hdrline);

		/* Add Subject: prefix if wanted */
		if(prefix) {
			if(strncasecmp(hdrline, "Subject:", 8) == 0) {
				subject_present = 1;
				unqp = cleanquotedp(hdrline + 8);
				if(strstr(hdrline + 8, prefix) == NULL &&
				   strstr(unqp, prefix) == NULL) {
					subject = concatstr(4,
							"Subject: ", prefix,
							hdrline + 8, "\n");
					writen(outfd, subject,
							strlen(subject));
					myfree(subject);
					myfree(hdrline);
					myfree(unqp);
					continue;
				}
				myfree(unqp);
			}
		}

		/* munge the From: and Reply-To: headers if so configured */
		if(listaddr != NULL) {
		    if(strncasecmp(hdrline, "Reply-To:", 9) == 0) {
			replyto_present = 1;
			writen(outfd, unfolded, strlen(unfolded));
			myfree(replyto);
			myfree(hdrline);
			continue;
		    }
		    if(strncasecmp(hdrline, "From:", 5) == 0) {
			replyto = concatstr(3,
					"Reply-To: ",
					hdrline + 5, "\n");
			from = concatstr(3,
					"From: ",
					listaddr, "\n");
			writen(outfd, from, strlen(from));
			myfree(from);
			myfree(hdrline);
			continue;
		    }
		}

		/* Should it be stripped? */
		if(!delhdrs || !findit(hdrline, delhdrs))
			writen(outfd, unfolded, strlen(unfolded));

		myfree(hdrline);
		myfree(unfolded);
	}

	/* Just print the rest of the mail */
	if(dumpfd2fd(infd, outfd) < 0) {
		log_error(LOG_ARGS, "Error when dumping rest of mail");
		return -1;
	}

	/* No more, let's add the footer if one */
	if(footfd >= 0)
		if(dumpfd2fd(footfd, outfd) < 0) {
			log_error(LOG_ARGS, "Error when adding footer");
			return -1;
		}

	fsync(outfd);

	return 0;
}
