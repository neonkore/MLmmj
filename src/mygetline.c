#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mygetline.h"

char *mygetline(FILE *infile)
{
	char *buf = malloc(BUFSIZE);
	char *str = malloc(BUFSIZE);
	size_t lenbuf, lenstr, i = 1;
	
	buf[0] = str[0] = 0;
	for(;;) {
		if(fgets(buf, BUFSIZE, infile) != NULL) {
			if(i == 1) {
				free(buf);
				free(str);
				return NULL;
			} else {
				free(buf);
				return str;
			}
		}
		lenbuf = strlen(buf);
		lenstr = strlen(str);
		realloc(str, lenbuf + lenstr + 1);
		strcat(str, buf);
		if(!((lenbuf == BUFSIZE - 1) && (buf[BUFSIZE - 1] != 'n'))) {
			free(buf);
			return str;
		}
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

	free(str);
	return 0;
}
#endif
