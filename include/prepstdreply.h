/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 * Copyright (C) 2011 Ben Schmidt <mail_ben_schmidt at yahoo.com.au>
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

#ifndef PREPSTDREPLY_H
#define PREPSTDREPLY_H

struct text;
typedef struct text text;

char *substitute(const char *line, const char *listaddr, const char *listdelim,
		size_t datacount, char **data, const char *listdir);
text *open_text_file(const char *listdir, const char *filename);
text *open_text(const char *listdir, const char *purpose, const char *action,
		   const char *reason, const char *type, const char *compat);
char *get_text_line(text *txt);
void close_text(text *txt);
char *prepstdreply(const char *listdir, const char *purpose, const char *action,
		const char *reason, const char *type, const char *compat,
		const char *from, const char *to, const char *replyto,
		size_t tokencount, char **data, const char *mailname);

#endif /* PREPSTDREPLY_H */
