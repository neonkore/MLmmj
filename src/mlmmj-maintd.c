/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include "mlmmj-maintd.h"
#include "mlmmj.h"
#include "log_error.h"

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/listdir [-F]\n"
	       " -L: Full path to list directory\n"
	       " -F: Don't fork, performing one maintenance run only.\n"
	       "     This option should be used when one wants to\n"
	       "     avoid running another daemon, and use e.g."
	       "     cron to control it instead.\n", prg);
	exit(EXIT_SUCCESS);
}

int clean_moderation(const char *listdir)
{
#if 0
	DIR *moddir;
	struct dirent *dp;
#endif

	/* TODO: Go through the moderation/ directory and delete mails
	 * older than MODREQLIFE (control/modreqlife later on)
	 */
		
	return 0;
}

int resend_queue(const char *listdir)
{
#if 0
	DIR *queuedir;
	struct dirent *dp;
#endif

	/* TODO: Go through all mails sitting in queue and send all that
	 * has a .mailfrom, .reciptto suffix.
	 */

	return 0;
}

int probe_bouncers(const char *listdir)
{
#if 0
	DIR *bouncedir;
	struct dirent *dp;
#endif

	/* TODO: invoke mlmmj-bounce -p address for all that haven't been
	 * probed in PROBEINTERVAL (control/probeinterval) seconds
	 */
	
	return 0;
}

int unsub_bouncers(const char *listdir)
{
#if 0
	DIR *bouncedir;
	struct dirent *dp;
#endif

	/* TODO: Unsubscribe all that still bounces after BOUNCELIFE time
	 * (control/bouncelife later on )
	 */

	return 0;
}

int main(int argc, char **argv)
{
	int opt, daemonize = 1;
	char *listdir = NULL;
	
	log_set_name(argv[0]);

	while ((opt = getopt(argc, argv, "hFVL:")) != -1) {
		switch(opt) {
		case 'F':
			daemonize = 0;
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'V':
			print_version(argv[0]);
			exit(0);
		}
	}

	if(listdir == NULL) {
		fprintf(stderr, "You have to specify -L\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if(chdir(listdir) < 0) {
		log_error(LOG_ARGS, "Could not chdir(%s), exiting. "
				    "No maintenance performed.", listdir);
		exit(EXIT_FAILURE);
	}

	if(daemonize) {
		if(daemon(1,0) < 0) {
			log_error(LOG_ARGS, "Could not daemonize. Only one "
					    "maintenance run will be done.");
			daemonize = 0;
		}
	}

	for(;;) {
		clean_moderation(listdir);
		resend_queue(listdir);
		probe_bouncers(listdir);
		unsub_bouncers(listdir);
		
		if(daemonize == 0)
			break;
		else
			sleep(MAINTD_SLEEP);
	}
		
	exit(EXIT_SUCCESS);
}
