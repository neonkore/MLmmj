/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef GET_LINE_H
#define GET_LINE_H

/* read the next line in infile, returns 0 when no more. */

char *get_line(char *buf,int count, int fd);

#endif /* GET_LINE_H */
