/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef READLN_H
#define READLN_H

#include <sys/types.h>

ssize_t readln(int fd, char *buf, size_t bufsize);

#endif /* READLN_H */
