#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef USE_SYSLOG
#include <syslog.h>
#endif


static char *log_name = NULL;


static void log_set_name(const char* name)
{
	if (log_name) free(log_name);
	log_name = strdup(name);
}


static void log_error(const char* msg)
{
	if (!log_name) log_name = "mlmmj-UNKNOWN";

#ifdef USE_SYSLOG
	static int syslog_is_open = 0;
	if(!syslog_is_open) {
		openlog(log_name, LOG_PID|LOG_CONS, LOG_MAIL);
		syslog_is_open = 1;
	}
	syslog(LOG_ERR, "%s: %s", msg, strerror(errno));
#else
	fprintf(stderr, "%s[%d]: %s: %s\n", log_name, (int)getpid(), msg,
			strerror(errno));
#endif
}
