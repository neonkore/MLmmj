/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef WRAPPERS_H
#define WRAPPERS_H

#include <sys/types.h>

ssize_t writen(int fd, const void *vptr, size_t n);
int random_int(void);

#endif /* WRAPPERS_H */
