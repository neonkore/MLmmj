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
#define MODREQLIFE 604800 /* How long time will moderation requests be kept?
			   * 604800s is 7 days */
#define DISCARDEDLIFE 604800 /* How long time will discarded mails be kept?
			      * 604800s is 7 days */
#define BOUNCELIFE 432000 /* How long time can addresses bounce before
			     unsubscription happens? 432000s is 5 days */
#define PROBEINTERVAL 43200 /* How often is an address probed if it still
			       bounces? 43200s is 12 hours */
#define MAINTD_SLEEP 3600 /* How long between maintenance runs when
			     mlmmj-maintd runs daemonized? 3600s is 1 hour */

struct mailhdr {
	const char *token;
	int valuecount;
	char **values;
};

void print_version(const char *prg);

#define MY_ASSERT(expression) if (!(expression)) { \
			log_error(LOG_ARGS, "assertion failed"); \
			exit(EXIT_FAILURE); \
		}


#endif /* MLMMJ_GENERIC_INCLUDES */
