/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef MLMMJ_UNSUBSCRIBE_H
#define MLMMJ_UNSUBSCRIBE_H

void confirm_unsub(const char *listdir, const char *listaddr,
		   const char *subaddr, const char *mlmmj);
void unsubscribe(int subreadfd, int subwritefd, const char *address);
void generate_unsubconfirm(const char *listdir, const char *listaddr,
			   const char *subaddr, const char *mlmmjsend);

#endif /* MLMMJ_UNSUBSCRIBE_H */
