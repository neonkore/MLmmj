/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "strip_file_to_fd.h"
#include "mlmmj.h"
#include "wrappers.h"


char *strip_headers(char *buf, const char **headers)
{
	int i = 0;

	if(buf[0] == '\t' || headers == 0)
		return buf;
	while(headers[i]) {
		if(strncmp(buf, headers[i], strlen(headers[i])) == 0)
			buf[0] = 0;
		i++;
	}
	return buf;
}

/* This function just adds the custom lines such as header, footer */
int custom_lines(int fd, FILE *infile)
{
	int ch;
	
	while((ch = fgetc(infile)) != -1)
		writen(fd, &ch, 1);
	
	return 0;
}

/* strip_file_to_fd reads a complete mail from in_fd, strips all headers in
   headers_to_strip from it, and saves it to out_fd. If headerfile is not 0 it
   will write this file in the headers. If we meet the To: header, we copy it
   to toheader.
 */
void strip_file_to_fd(FILE *in, int out_fd, const char **headers_to_strip,
		    FILE *headerfile, FILE *footerfile, char *toheader)
{
	char buf[READ_BUFSIZE];
	signed char readbuf[2];
	int bytes_written;
	int buf_count = 0;
	
	memset(buf, 0, READ_BUFSIZE);
	while((readbuf[0]=fgetc(in)) != -1) {
		if(readbuf[0] == '\n') {
			readbuf[1]=fgetc(in);
			if(readbuf[1] != -1)
				ungetc(readbuf[1], in);
			else if(ferror(in))
				exit(EXIT_FAILURE);
		}
		if(readbuf[0] != '\n')
			buf[buf_count++] = readbuf[0];
		else if(readbuf[1] == '\t') {
			buf[buf_count++] = readbuf[0];
			buf[buf_count] = readbuf[1];
			memset(readbuf, 0, 2);
		} else { /* readbuf[0] == '\n' */
			if (readbuf[1] == '\n' && headerfile)
				custom_lines(out_fd, headerfile);
			buf[buf_count] = readbuf[0];
			if(buf_count == 0) /* headers are done */
				headers_to_strip = 0;
				/* XXX:free_str_array(headers_to_strip);*/
			if(headers_to_strip) {/* only strip if still headers */
				strip_headers(buf, headers_to_strip);
				if(toheader && (strncmp(buf, "To: ", 4) == 0))
					sprintf(toheader, "%s", buf);
			}
			if(buf[0]) {
				bytes_written = writen(out_fd, buf, strlen(buf));
				if(bytes_written == -1) {
					perror(":error writing to fd");
					exit(EXIT_FAILURE);
				}
			}
			memset(buf, 0, READ_BUFSIZE);
			buf_count = 0;
		}
	}
	if(footerfile)
		custom_lines(out_fd, footerfile);

	if(ferror(in))
		exit(EXIT_FAILURE);
}

