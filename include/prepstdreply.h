/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef PREPSTDREPLY_H
#define PREPSTDREPLY_H

char *prepstdreply(const char *listdir, const char *filename, const char *from,
		   const char *to, const char *replyto, const char *subject,
		   size_t tokencount, char **data);

#endif /* PREPSTDREPLY_H */
