/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef MYLOCKING_H
#define MYLOCKING_H

int myexcllock(int fd);
int myunlock(int fd);

#endif /* MYLOCKING_H */
