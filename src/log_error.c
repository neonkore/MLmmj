#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "log_error.h"
#include "../config.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

char *log_name = NULL;

void log_set_name(const char* name)
{
	if (log_name) free(log_name);
	log_name = strdup(name);
}

void log_error_do(const char *msg, const char *file, int line)
{
	static int syslog_is_open = 0;
	if (!log_name) log_name = "mlmmj-UNKNOWN";
#ifdef HAVE_SYSLOG
	if(!syslog_is_open) {
		openlog(log_name, LOG_PID|LOG_CONS, LOG_MAIL);
		syslog_is_open = 1;
	}
	syslog(LOG_ERR, "%s:%d: %s: %s", file, line, msg, strerror(errno));
#else
	fprintf(stderr, "%s[%d]: %s:%d: %s: %s\n", log_name, (int)getpid(),
			file, line,
			msg, strerror(errno));
#endif
}
