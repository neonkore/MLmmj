#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "prepstdreply.h"
#include "strgen.h"
#include "chomp.h"
#include "log_error.h"
#include "mygetline.h"
#include "wrappers.h"

char *prepstdreply(const char *listdir, const char *filename, const char *from,
		   const char *to, const char *replyto, const char *subject,
		   size_t tokencount, char **data)
{
	int infd, outfd;
	size_t i;
	char *str, *tmp, *retstr;

	tmp = concatstr(3, listdir, "/text/", filename);
	infd = open(tmp, O_RDONLY);
	free(tmp);
	if(infd < 0) {
		log_error(LOG_ARGS, "Could not open std mail %s", filename);
		return NULL;
	}

	tmp = concatstr(6, "From: ", from, "\nTo: ", to, "\nSubject: ", subject);
	if(replyto)
		str = concatstr(3, tmp, "\nReply-To: ", replyto, "\n\n");
	else
		str = concatstr(2, tmp, "\n\n");
		
	free(tmp);

	tmp = random_str();
	retstr = concatstr(3, listdir, "/queue/", random);
	free(tmp);
	outfd = open(retstr, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	if(outfd < 0) {
		log_error(LOG_ARGS, "Could not open std mail %s", tmp);
		return NULL;
	}

	if(writen(outfd, str, strlen(str)) < 0) {
		log_error(LOG_ARGS, "Could not write std mail");
		return NULL;
	}
	free(str);

	while((str = mygetline(infd))) {
		for(i = 0; i < tokencount; i++) {
			if(strncmp(str, data[i*2], strlen(data[i*2])) == 0) {
				free(str);
				str = strdup(data[(i*2)+1]);
			}
		}
		if(writen(outfd, str, strlen(str)) < 0) {
			log_error(LOG_ARGS, "Could not write std mail");
			return NULL;
		}
		free(str);
	}
	
	fsync(outfd);
	close(outfd);

	return retstr;
}	
