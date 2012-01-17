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

typedef void (*rewind_function)(void *state);
typedef const char *(*get_function)(void *state);

struct memory_lines_state;
typedef struct memory_lines_state memory_lines_state;

struct file_lines_state;
typedef struct file_lines_state file_lines_state;


memory_lines_state *init_memory_lines(const char *lines);
void rewind_memory_lines(void *state);
const char *get_memory_line(void *state);
void finish_memory_lines(memory_lines_state *s);

file_lines_state *init_file_lines(const char *filename, int open_now);
file_lines_state *init_truncated_file_lines(const char *filename, int open_now,
		char truncate);
void rewind_file_lines(void *state);
const char *get_file_line(void *state);
void finish_file_lines(file_lines_state *s);

char *substitute(const char *line, const char *listaddr, const char *listdelim,
		const char *listdir, text *txt);

text *open_text_file(const char *listdir, const char *filename);
text *open_text(const char *listdir, const char *purpose, const char *action,
		   const char *reason, const char *type, const char *compat);
void register_unformatted(text *txt, const char *token, const char *subst);
void register_originalmail(text *txt, const char *mailname);
void register_formatted(text *txt, const char *token,
		rewind_function rew, get_function get, void * state);
char *get_processed_text_line(text *txt,
		const char *listaddr, const char *listdelim, const char *listdir);
char *prepstdreply(text *txt, const char *listdir,
		const char *from, const char *to, const char *replyto);
void close_text(text *txt);

#endif /* PREPSTDREPLY_H */
