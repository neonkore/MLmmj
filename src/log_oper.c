#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mlmmj.h"
#include "log_error.h"
#include "log_oper.h"

int log_oper(const char *logfilename, const char *str)
{
	char ct[26], *logstr;
	struct stat st;
	time_t t;

	if(lstat(logfilename, &st) < 0 && errno != ENOENT) {
		log_error(LOG_ARGS, "Could not stat logfile %s", logfilename);
		return -1;
	} else if((st.st_mode & S_IFMT) == S_IFLNK) {
		log_error(LOG_ARGS, "%s is a symbolic link, not opening",
					logfilename);
		return -1;
	}
	
	fd = open(logfilename, O_RDWR|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
	if(fd < 0) {
		log_error(LOG_ARGS, "Could not open %s", logfilename);
		return -1;
	}

	if((time(&t) == (time_t)-1) || (ctime_r(&t, ct) == NULL))
		strncpy(ct, "Unknown time", sizeof(ct));

	if(myexcllock(fd) < 0) {
		log_error(LOG_ARGS, "Could not lock %s", logfilename);
		return -1;
	}

	logstr = concatstr(4, ct, ":", str, "\n");
	if(writen(fd, logstr, strlen(logstr)) < 0)
		log_error(LOG_ARGS, "Could not write to %s", logfilename);
	
	if(myunlock(fd) < 0)
		log_error(LOG_ARGS, "Could not unlock %s", logfilename);

	close(fd);
	myfree(logstr);
	
	return 0;
}
