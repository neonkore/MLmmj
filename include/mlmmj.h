/* Copyright (C) 2002, 2003, 2004 Mads Martin Joergensen <mmj at mmj.dk>
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
#define READ_BUFSIZE 2048
#define RECIPDELIM '+'
#define MAX_CONNECTIONS 3 /* How many max connections to relayhost */

struct mailhdr {
	const char *token;
	char *value;
};

void print_version(const char *prg);

#define MY_ASSERT(expression) if (!(expression)) { \
			log_error(LOG_ARGS, "assertion failed"); \
			exit(EXIT_FAILURE); \
		}


#endif /* MLMMJ_GENERIC_INCLUDES */
