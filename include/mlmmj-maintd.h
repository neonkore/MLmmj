/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef MLMMJ_MAINTD_H
#define MLMMJ_MAINTD_H

int clean_moderation(const char *listdir);
int resend_queue(const char *listdir);
int probe_bouncers(const char *listdir);
int unsub_bouncers(const char *listdir);

#endif /* MLMMJ_MAINTD_H */
