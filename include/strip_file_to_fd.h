/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef STRIP_FILE_TO_FD_H
#define STRIP_FILE_TO_FD_H

#include <stdio.h>

char *strip_headers(char *readbuf, const char **headers_to_strip);
int custom_lines(int fd, FILE *infile);
void strip_file_to_fd(FILE *in, int out_fd, const char **headers_to_strip,
		    FILE *headerfile, FILE *footerfile, char *toheader);

#endif /* STRIP_FILE_TO_FD_H */
