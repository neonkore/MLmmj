/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef CHECK_REPLY_H
#define CHECK_REPLY_H

#define MLMMJ_CONNECT 1
#define MLMMJ_HELO 2
#define MLMMJ_FROM 4
#define MLMMJ_RCPTTO 8
#define MLMMJ_DATA 16
#define MLMMJ_DOT 32
#define MLMMJ_QUIT 64
#define MLMMJ_RSET 128

#include "mlmmj.h"

char *checkwait_smtpreply(int sockfd, int replytype);

#endif /* CHECK_REPLY_H */
