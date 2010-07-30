/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "wrappers.h"

int random_int()
{
	static int init = 0;
	unsigned int seed;
	int devrandom;
	unsigned char ch;

	if (init) return rand();

	seed = (unsigned int)time(NULL);

	devrandom = open("/dev/urandom", O_RDONLY);
	if(devrandom < 0)
		devrandom = open("/dev/random", O_RDONLY);

	if (devrandom >= 0) {
		readn(devrandom, &ch, 1);
		seed ^= ch;
		readn(devrandom, &ch, 1);
		seed ^= ch << 8;
		readn(devrandom, &ch, 1);
		seed ^= ch << 16;
		readn(devrandom, &ch, 1);
		seed ^= ch << 24;
		close(devrandom);
	}

	srand(seed);
	init = 1;

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
