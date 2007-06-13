/* Copyright (C) 2005 Morten K. Poulsen <morten at afdelingp.dk>
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

#ifndef UNISTR_H
#define UNISTR_H

typedef unsigned int unistr_char;

typedef struct _unistr {
	size_t len;
	size_t alloc_len;
	unistr_char *chars;
} unistr;

unistr *unistr_new(void);
void unistr_free(unistr *str);
int unistr_cmp(unistr *str1, unistr *str2);
unistr *unistr_dup(unistr *str);
void unistr_append_char(unistr *str, unistr_char uc);
void unistr_append_usascii(unistr *str, char *binary, size_t bin_len);
void unistr_append_utf8(unistr *str, char *binary, size_t bin_len);
void unistr_append_iso88591(unistr *str, char *binary, size_t bin_len);
void unistr_dump(unistr *str);
char *unistr_to_utf8(unistr *str);
char *unistr_header_to_utf8(char *str);
char *unistr_utf8_to_header(char *str);
char *unistr_escaped_to_utf8(char *str);

#endif
