/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef MLMMJ_SUBSCRIBE_H
#define MLMMJ_SUBSCRIBE_H

void unsubscribe(int subreadfd, int subwritefd, const char *address);
void generate_subconfirm(const char *listdir, const char *listadr,
		const char *subaddr);

#endif /* MLMMJ_SUBSCRIBE_H */
