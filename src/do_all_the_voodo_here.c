/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
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

int findit(const char *line, const char **headers)
{
	int i = 0;
	size_t len;

	while(headers[i]) {
		len = strlen(headers[i]);
		if(strncmp(line, headers[i], len) == 0)
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
		if(strncmp(line, readhdrs[i].token, tokenlen) == 0) {
			readhdrs[i].valuecount++;
			valuelen = linelen - tokenlen + 1;
			readhdrs[i].values =
				(char **)realloc(readhdrs[i].values,
				  readhdrs[i].valuecount * sizeof(char *));
			readhdrs[i].values[readhdrs[i].valuecount - 1] =
					(char *)malloc(valuelen + 1);
			strncpy(readhdrs[i].values[readhdrs[i].valuecount - 1],
						line+tokenlen, valuelen);
			chomp(readhdrs[i].values[readhdrs[i].valuecount - 1]);
		}
		i++;
	}
}

int do_all_the_voodo_here(int infd, int outfd, int hdrfd, int footfd,
		 const char **delhdrs, struct mailhdr *readhdrs,
		 const char *prefix)
{
	char *hdrline, *subject;

	while((hdrline = gethdrline(infd))) {
		/* Done with headers? Then add extra if wanted*/
		if((strlen(hdrline) == 1) && (hdrline[0] == '\n')){
			if(hdrfd) {
				if(dumpfd2fd(hdrfd, outfd) < 0) {
					log_error(LOG_ARGS, "Could not"
						"add extra headers");
					free(hdrline);
					return -1;
				}
			}
			write(outfd, hdrline, strlen(hdrline));
			fsync(outfd);
			free(hdrline);
			break;
		}
		/* Do we want info from hdrs? Get it before it's gone */
		if(readhdrs)
			getinfo(hdrline, readhdrs);

		/* Add Subject: prefix if wanted */
		if(prefix) {
			if(strncmp(hdrline, "Subject: ", 9) == 0) {
				if(strstr(hdrline + 9, prefix) == NULL) {
					subject = concatstr(4,
							"Subject: ", prefix,
							" ", hdrline + 9);
					writen(outfd, subject,
							strlen(subject));
					free(subject);
					free(hdrline);
					continue;
				}
			}
		}
		
		/* Should it be stripped? */
		if(delhdrs) {
			if(!findit(hdrline, delhdrs))
				writen(outfd, hdrline, strlen(hdrline));
		} else
			writen(outfd, hdrline, strlen(hdrline));


		free(hdrline);
	}

	/* Just print the rest of the mail */
	if(dumpfd2fd(infd, outfd) < 0) {
		log_error(LOG_ARGS, "Error when dumping rest of mail");
		return -1;
	}

	/* No more, lets add the footer if one */
	if(footfd)
		if(dumpfd2fd(footfd, outfd) < 0) {
			log_error(LOG_ARGS, "Error when adding footer");
			return -1;
		}

	fsync(outfd);

	return 0;
}
