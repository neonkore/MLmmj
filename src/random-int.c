/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int random_int()
{
	unsigned int seed;
	FILE *devrandom;

	seed = (unsigned int)time(NULL);

	devrandom = fopen("/dev/urandom", "r");
	if(!devrandom)
		devrandom = fopen("/dev/random", "r");

	if (devrandom) {
		seed ^= ((unsigned char)fgetc(devrandom));
		seed ^= ((unsigned char)fgetc(devrandom)) << 8;
		seed ^= ((unsigned char)fgetc(devrandom)) << 16;
		seed ^= ((unsigned char)fgetc(devrandom)) << 24;
		fclose(devrandom);
	}

	srand(seed);

	return rand();
}
