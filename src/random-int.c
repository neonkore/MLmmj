/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int random_int()
{
	unsigned int seed;
	int devrandom;
	unsigned char ch;

	seed = (unsigned int)time(NULL);

	devrandom = open("/dev/urandom", O_RDONLY);
	if(devrandom < 0)
		devrandom = open("/dev/random", O_RDONLY);

	if (devrandom >= 0) {
		read(devrandom, &ch, 1);
		seed ^= ch;
		read(devrandom, &ch, 1);
		seed ^= ch << 8;
		read(devrandom, &ch, 1);
		seed ^= ch << 16;
		read(devrandom, &ch, 1);
		seed ^= ch << 24;
		close(devrandom);
	}

	srand(seed);

	return rand();
}
#if 0
int main(int argc, char **argv)
{
	int i;
	
	for(i = 0; i < 25; i++)
		printf("%i\n", random_int());
	
	return 0;
}
#endif
