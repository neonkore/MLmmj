#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mygetline.h"

char *mygetline(FILE *infile)
{
	size_t buf_size = BUFSIZE;  /* initial buffer size */
	size_t buf_used;
	char *buf = malloc(buf_size);
	
	buf[0] = '\0';
	for (;;) {
		buf_used = strlen(buf);
		if (fgets(buf+buf_used, buf_size-buf_used, infile) == NULL) {
			if (buf[0]) {
				 return buf;
			} else {
				free(buf);
				return NULL;
			}
		}

		if ((strlen(buf) < buf_size-1) || (buf[buf_size-1] == '\n')) {
			return buf;
		}

		/* grow buffer */
		buf_size *= 2;
		buf = realloc(buf, buf_size);

	}
}
#if 0
int main(int argc, char **argv)
{
	char *str;
	
	while((str = mygetline(stdin))) {
		printf("%s", str);
		free(str);
	}

	return 0;
}
#endif
