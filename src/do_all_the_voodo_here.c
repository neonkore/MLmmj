/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mlmmj.h"
#include "mygetline.h"
#include "gethdrline.h"
#include "strgen.h"
#include "chomp.h"
#include "ctrlvalue.h"
#include "do_all_the_voodo_here.h"

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

void do_all_the_voodo_here(FILE *in, FILE *out, FILE *hdradd, FILE *footers,
		 const char **delhdrs, struct mailhdr *readhdrs,
		 const char *prefix)
{
	char *hdrline, *line, *subject;

	while((hdrline = gethdrline(in))) {
		/* Done with headers? Then add extra if wanted*/
		if((strlen(hdrline) == 1) && (hdrline[0] == '\n')){
			if(hdradd) {
				fflush(out);
				while((line = myfgetline(hdradd))) {
					fputs(line, out);
					free(line);
				}
				fflush(out);
			}
			fputs(hdrline, out);
			fflush(out);
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
					fputs(subject, out);
					free(subject);
					free(hdrline);
					continue;
				}
			}
		}
		
		/* Should it be stripped? */
		if(delhdrs) {
			if(!findit(hdrline, delhdrs))
				fputs(hdrline, out);
		} else
			fputs(hdrline, out);


		free(hdrline);
	}

	/* Just print the rest of the mail */
	while((line = myfgetline(in))) {
		fputs(line, out);
		free(line);
	}

	fflush(out);

	/* No more, lets add the footer if one */
	if(footers) {
		while((line = myfgetline(footers))) {
			fputs(line, out);
			free(line);
		}
	}

	fflush(out);
}
#if 0
int main(int argc, char **argv)
{
	int i = 0;
	FILE *hdrfile = fopen(argv[1], "r");
	FILE *footfile = fopen(argv[2], "r");
	const char *badhdrs[] = {"From ", "Received:", NULL};
	struct mailhdr readhdrs[] = {
		{"MIME-Version: ", NULL},
		{"Date: ", NULL},
		{ NULL , NULL }
	};
	
	do_all_the_voodo_here(stdin, stdout, hdrfile, footfile, badhdrs,
 				readhdrs);

	while(readhdrs[i].token) {
		printf(	"readhdrs[%d].token = [%s]\n"
			"readhdrs[%d].value = [%s]\n", i, readhdrs[i].token,
						     i, readhdrs[i].value);
		i++;
	}

	return 0;
}
#endif
