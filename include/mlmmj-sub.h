/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef MLMMJ_SUBSCRIBE_H
#define MLMMJ_SUBSCRIBE_H

void confirm_sub(const char *listdir, const char *listaddr,
		 const char *subaddr, const char *mlmmjsend);
void generate_subconfirm(const char *listdir, const char *listadr,
		const char *subaddr, const char *mlmmjsend);

#endif /* MLMMJ_SUBSCRIBE_H */
