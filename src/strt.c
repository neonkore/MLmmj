#include "../include/strgen.h"

int main()
{
	char *str = strgen(3, "fick ", "dich!", "\n");
	printf("[%s]", str);
	free(str);

	return 0;
}
