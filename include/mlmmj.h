/* Copyright (C) 2002, 2003, 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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
#define WAITPROBE 43200   /* How long do we wait for a bounce of the probe
			     mail before concluding the address is no longer
			     bouncing? 43200 is 12 hours */
#define MAINTD_SLEEP 7200 /* How long between maintenance runs when
			     mlmmj-maintd runs daemonized? 7200s is 2 hours */
#define MAINTD_LOGFILE "mlmmj-maintd.lastrun.log"

struct strlist {
	int count;
	char **strs;
};

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

#define CHECKFULLPATH(name) if(strchr(name, '/') == NULL) { \
			fprintf(stderr, "All mlmmj binaries have to " \
					"be invoked with full path,\n" \
					"e.g. /usr/local/bin/%s\n", name); \
			exit(EXIT_FAILURE); \
			};

/* make sure we use the wrappers */
#ifndef _MEMORY_C
#define malloc	Bad_programmer__no_biscuit
#define realloc	Bad_programmer__no_biscuit
#define free	Bad_programmer__no_biscuit
#define strdup	Bad_programmer__no_biscuit
#endif  /* _MEMORY_C */

#endif /* MLMMJ_GENERIC_INCLUDES */
