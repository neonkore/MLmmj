/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef HEADER_TOKEN_H
#define HEADER_TOKEN_H

char *find_header(char *buf, const char *token);
char *find_header_file(FILE *infile, char *scanbuf, const char *header_token);

#endif
