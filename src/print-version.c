#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "mlmmj.h"

void print_version(const char *prg)
{
	char *lastslashindex = rindex(prg, '/');
	
	if(!lastslashindex)
		lastslashindex = (char *)prg;
	else
		lastslashindex++;

	printf("%s version "VERSION"\n", lastslashindex);	
}
