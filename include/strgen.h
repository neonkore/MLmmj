/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef STRGEN_H
#define STRGEN_H

char *random_str(void);
char *random_plus_addr(const char *addr);
char *headerstr(const char *headertoken, const char *str);
char *genlistname(const char *listaddr);
char *genlistfqdn(const char *listaddr);
char *concatstr(int count, ...);

#endif /* STRGEN_H */
