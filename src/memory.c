/* Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
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

/* define _MEMORY_C so we can use the libc memory functions -- see mlmmj.h */
#define _MEMORY_C	1

#include <stdlib.h>
#include <string.h>

#include "mlmmj.h"
#include "memory.h"
#include "log_error.h"


/* TODO add a debug mode with canaries and other goodies */


void *__mymalloc(const char *file, int line, size_t size)
{
	void *ret;
	
	ret = malloc(size);
	if (ret == NULL) {
		log_error(file, line, strerror(errno),
				"malloc(%d) failed! Bailing out!", size);
		exit(EXIT_FAILURE);
	}

	return ret;
}


void *__myrealloc(const char *file, int line, void *ptr, size_t size)
{
	void *ret;
	
	ret = realloc(ptr, size);
	if (ret == NULL) {
		log_error(file, line, strerror(errno),
				"realloc(%p, %d) failed! Bailing out!",
				ptr, size);
		exit(EXIT_FAILURE);
	}

	return ret;
}


void __myfree(const char *file, int line, void *ptr)
{
	free(ptr);
}


char *__mystrdup(const char *file, int line, const char *str)
{
	void *ret;
	
	ret = strdup(str);
	if (ret == NULL) {
		log_error(file, line, strerror(errno),
				"strdup() failed! Bailing out!");
		exit(EXIT_FAILURE);
	}

	return ret;
}
