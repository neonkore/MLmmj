/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef MLMMJ_GENERIC_INCLUDES
#define MLMMJ_GENERIC_INCLUDES

#include "../config.h"

#define RELAYHOST "127.0.0.1"

/* must end in a slash, if not empty! */
#ifndef BINDIR
#define BINDIR ""
#endif


#define READ_BUFSIZE 2048
#define RECIPDELIM '+'

struct mailhdr {
	const char *token;
	char *value;
};

void print_version(const char *prg);

#endif /* MLMMJ_GENERIC_INCLUDES */
