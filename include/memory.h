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

#ifndef _MEMORY_H
#define _MEMORY_H	1

void *__mymalloc(const char *file, int line, size_t size);
void *__myrealloc(const char *file, int line, void *ptr, size_t size);
void __myfree(const char *file, int line, void *ptr);
char *__mystrdup(const char *file, int line, const char *str);

#define mymalloc(s) __mymalloc(__FILE__, __LINE__, s)
#define myrealloc(p,s) __myrealloc(__FILE__, __LINE__, p, s)
#define myfree(p) __myfree(__FILE__, __LINE__, p)
#define mystrdup(p) __mystrdup(__FILE__, __LINE__, p)

#endif  /* _MEMORY_H */
