#include <stdio.h>
#include <stdlib.h>

#include "mygetline.h"
#include "gethdrline.h"
#include "strgen.h"

char *gethdrline(FILE *infile)
{
	char *line = NULL, *retstr = NULL, *nextline = NULL, *tmp = NULL;
	int ch;
	
	for(;;) {
		line = mygetline(infile);
		if(line == NULL)
			return NULL;
		ch = getc(infile); ungetc(ch, infile);
		if(ch == '\t' || ch == ' ') {
			nextline = mygetline(infile);
			tmp = retstr;
			retstr = concatstr(3, retstr, line, nextline);
			free(tmp); free(line); free(nextline);
			tmp = line = nextline = NULL;
			ch = getc(infile); ungetc(ch, infile);
			if(!(ch == '\t' || ch == ' '))
				return retstr;
		} else {
			tmp = retstr;
			retstr = concatstr(3, retstr, line, nextline);
			free(tmp);

			return retstr;
		}
	}
}
#if 0
int main(int argc, char **argv)
{
	char *str;

	while((str = gethdrline(stdin))) {
		printf("%s", str);
		free(str);
	}

	free(str);

	return 0;
}
#endif
