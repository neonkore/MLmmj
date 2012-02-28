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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>

#include "mlmmj.h"
#include "unistr.h"
#include "log_error.h"
#include "memory.h"


unistr *unistr_new(void)
{
	unistr *ret;

	ret = mymalloc(sizeof(unistr));
	ret->len = 0;
	ret->alloc_len = 64;
	ret->chars = mymalloc(ret->alloc_len * sizeof(unistr_char));

	return ret;
}


void unistr_free(unistr *str)
{
	if (!str)
		return;
	myfree(str->chars);
	myfree(str);
}


int unistr_cmp(const unistr *str1, const unistr *str2)
{
	unsigned int i;

	for (i=0; i<str1->len; i++) {
		if (str1->chars[i] < str2->chars[i]) {
			return -1;
		} else if (str1->chars[i] > str2->chars[i]) {
			return 1;
		}
	}
	if (str2->len > str1->len) {
		return 1;
	}
	return 0;
}


unistr *unistr_dup(const unistr *str)
{
	unistr *ret;
	unsigned int i;

	ret = unistr_new();
	for (i=0; i<str->len; i++) {
		unistr_append_char(ret, str->chars[i]);
	}

	return ret;
}


void unistr_append_char(unistr *str, unistr_char uc)
{
	if (str->len >= str->alloc_len) {
		str->alloc_len *= 2;
		str->chars = myrealloc(str->chars, str->alloc_len * sizeof(unistr_char));
	}
	str->chars[str->len++] = uc;
}


void unistr_append_usascii(unistr *str, const char *binary, size_t bin_len)
{
	unsigned int i;

	for (i=0; i<bin_len; i++) {
		if ((unsigned char)binary[i] > 0x7F) {
			unistr_append_char(str, '?');
		} else {
			unistr_append_char(str, (unsigned char)binary[i]);
		}
	}
}


void unistr_append_utf8(unistr *str, const char *binary, size_t bin_len)
{
	unsigned int i, j;
	unistr_char ch;
	unsigned char *bin = (unsigned char *)binary;

	for (i=0; i<bin_len; i++) {
		if (bin[i] <= 0x7F) {  /* 1 */
			unistr_append_char(str, bin[i]);
		} else {
			if ((bin[i] & 224) == 192) {  /* 2 */
				ch = bin[i] & 31;
				j = 1;
			} else if ((bin[i] & 240) == 224) {  /* 3 */
				ch = bin[i] & 15;
				j = 2;
			} else if ((bin[i] & 248) == 240) {  /* 4 */
				ch = bin[i] & 7;
				j = 3;
			} else if ((bin[i] & 252) == 248) {  /* 5 */
				ch = bin[i] & 3;
				j = 4;
			} else if ((bin[i] & 254) == 252) {  /* 6 */
				ch = bin[i] & 1;
				j = 5;
			} else {
				/* invalid byte sequence */
				unistr_append_char(str, '?');
				continue;
			}
			if (ch == 0) {
				/* invalid encoding, no data bits set in first byte */
				unistr_append_char(str, '?');
				continue;
			}
			for (;j>0; j--) {
				i++;
				ch <<= 6;
				if ((bin[i] & 192) != 128) {
					/* invalid byte sequence */
					ch = '?';
					break;
				}
				ch |= bin[i] & 63;
			}
			unistr_append_char(str, ch);
		}
	}
}


void unistr_append_iso88591(unistr *str, const char *binary, size_t bin_len)
{
	unsigned int i;

	for (i=0; i<bin_len; i++) {
		if (binary[i] == 0x00) {
			unistr_append_char(str, '?');
		} else {
			unistr_append_char(str, (unsigned char)binary[i]);
		}
	}
}


void unistr_dump(const unistr *str)
{
	unsigned int i;

	printf("unistr_dump(%p)\n", (void *)str);
	printf(" ->len = %lu\n", (unsigned long)str->len);
	printf(" ->alloc_len = %lu\n", (unsigned long)str->alloc_len);
	printf(" ->chars [ ");
	for (i=0; i<str->len; i++) {
		if ((str->chars[i] <= 0x7F) && (str->chars[i] != '\n')) {
			printf("'%c' ", str->chars[i]);
		} else {
			printf("0x%02X ", str->chars[i]);
		}
	}
	printf("]\n");
}


char *unistr_to_utf8(const unistr *str)
{
	unsigned int i;
	size_t len = 0;
	char *ret;
	char *p;

	for (i=0; i<str->len; i++) {
		if (str->chars[i] <= 0x7F) {
			len++;
		} else if (str->chars[i] <= 0x7FF) {
			len += 2;
		} else if (str->chars[i] <= 0xFFFF) {
			len += 3;
		} else if (str->chars[i] <= 0x1FFFFF) {
			len += 4;
		} else if (str->chars[i] <= 0x3FFFFFF) {
			len += 5;
		} else if (str->chars[i] <= 0x7FFFFFFF) {
			len += 6;
		} else {
			errno = 0;
			log_error(LOG_ARGS, "unistr_to_utf8(): can not utf-8 encode"
					"U+%04X", str->chars[i]);
			return mystrdup("");
		}
	}
	len++;  /* NUL */

	ret = mymalloc(len);
	p = ret;

	for (i=0; i<str->len; i++) {
		if (str->chars[i] <= 0x7F) {  /* 1 */
			*(p++) = str->chars[i];
		} else if (str->chars[i] <= 0x7FF) {  /* 2 */
			*(p++) = 192 + ((str->chars[i] & 1984) >> 6);
			*(p++) = 128 + (str->chars[i] & 63);
		} else if (str->chars[i] <= 0xFFFF) {  /* 3 */
			*(p++) = 224 + ((str->chars[i] & 61440) >> 12);
			*(p++) = 128 + ((str->chars[i] & 4032) >> 6);
			*(p++) = 128 + (str->chars[i] & 63);
		} else if (str->chars[i] <= 0x1FFFFF) {  /* 4 */
			*(p++) = 240 + ((str->chars[i] & 1835008) >> 18);
			*(p++) = 128 + ((str->chars[i] & 258048) >> 12);
			*(p++) = 128 + ((str->chars[i] & 4032) >> 6);
			*(p++) = 128 + (str->chars[i] & 63);
		} else if (str->chars[i] <= 0x3FFFFFF) {  /* 5 */
			*(p++) = 248 + ((str->chars[i] & 50331648) >> 24);
			*(p++) = 128 + ((str->chars[i] & 16515072) >> 18);
			*(p++) = 128 + ((str->chars[i] & 258048) >> 12);
			*(p++) = 128 + ((str->chars[i] & 4032) >> 6);
			*(p++) = 128 + (str->chars[i] & 63);
		} else if (str->chars[i] <= 0x7FFFFFFF) {  /* 6 */
			*(p++) = 252 + ((str->chars[i] & 1073741824) >> 30);
			*(p++) = 128 + ((str->chars[i] & 1056964608) >> 24);
			*(p++) = 128 + ((str->chars[i] & 16515072) >> 18);
			*(p++) = 128 + ((str->chars[i] & 258048) >> 12);
			*(p++) = 128 + ((str->chars[i] & 4032) >> 6);
			*(p++) = 128 + (str->chars[i] & 63);
		} else {
			errno = 0;
			log_error(LOG_ARGS, "unistr_to_utf8(): can not utf-8 encode"
					"U+%04X", str->chars[i]);
		}
	}
	*(p++) = '\0';

	return ret;
}


static int hexval(char ch)
{
	ch = tolower(ch);

	if ((ch >= 'a') && (ch <= 'f')) {
		return 10 + ch - 'a';
	}

	if ((ch >= '0') && (ch <= '9')) {
		return ch - '0';
	}

	return 0;
}


static void decode_qp(char *str, char **binary, size_t *bin_len)
{
	int i;

	/* decoded string will never be longer, and we don't include a NUL */
	*binary = mymalloc(strlen(str));
	*bin_len = 0;

	for (i=0; str[i]; i++) {
		if ((str[i] == '=') && isxdigit(str[i+1]) && isxdigit(str[i+2])) {
			(*binary)[(*bin_len)++] = (hexval(str[i+1]) << 4) + hexval(str[i+2]);
			i += 2;
		} else if (str[i] == '_') {
			(*binary)[(*bin_len)++] = 0x20;
		} else {
			(*binary)[(*bin_len)++] = str[i];
		}
	}
}


static void decode_base64(char *str, char **binary, size_t *bin_len)
{
	int tab[] = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
		-1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
		15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
		-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	};
	size_t len;
	unsigned int i;
	unsigned int out;
	int out_numbits;
	int val;

	/* decoded string will never be longer, and we don't include a NUL */
	len = strlen(str);
	*binary = mymalloc(len);
	*bin_len = 0;

	out = 0;
	out_numbits = 0;
	for (i=0; i<strlen(str); i++) {
		val = tab[(unsigned char)str[i]];
		if (val == -1)
			continue;
		out <<= 6;
		out |= val;
		out_numbits += 6;
		if (out_numbits >= 8) {
			(*binary)[(*bin_len)++] = (out >> (out_numbits - 8)) & 255;
			out_numbits -= 8;
		}
	}
}


static void header_decode_word(char *word, unistr *ret)
{
	char *my_word;
	char *charset, *encoding, *string, *end;
	char *binary;
	size_t bin_len;


	if ((word[0] != '=') || (word[1] != '?')) {
		unistr_append_usascii(ret, word, strlen(word));
		return;
	}

	my_word = mystrdup(word);

	charset = my_word + 2;

	if ((encoding = strchr(charset, '?')) == NULL) {
		/* missing encoding */
		unistr_append_usascii(ret, "???", 3);
		myfree(my_word);
		return;
	}
	*(encoding++) = '\0';

	if ((string = strchr(encoding, '?')) == NULL) {
		/* missing string */
		unistr_append_usascii(ret, "???", 3);
		myfree(my_word);
		return;
	}
	*(string++) = '\0';

	if ((end = strchr(string, '?')) == NULL) {
		/* missing end */
		unistr_append_usascii(ret, "???", 3);
		myfree(my_word);
		return;
	}
	*(end++) = '\0';
	if ((end[0] != '=') || (end[1] != '\0')) {
		/* broken end */
		unistr_append_usascii(ret, "???", 3);
		myfree(my_word);
		return;
	}

	if (tolower(encoding[0]) == 'q') {
		decode_qp(string, &binary, &bin_len);
	} else if (tolower(encoding[0]) == 'b') {
		decode_base64(string, &binary, &bin_len);
	} else {
		/* unknown encoding */
		unistr_append_usascii(ret, "???", 3);
		myfree(my_word);
		return;
	}

	if (strcasecmp(charset, "us-ascii") == 0) {
		unistr_append_usascii(ret, binary, bin_len);
	} else if (strcasecmp(charset, "utf-8") == 0) {
		unistr_append_utf8(ret, binary, bin_len);
	} else if (strcasecmp(charset, "iso-8859-1") == 0) {
		unistr_append_iso88591(ret, binary, bin_len);
	} else {
		/* unknown charset */
		unistr_append_usascii(ret, "???", 3);
	}

	myfree(my_word);
	myfree(binary);
}


/* IN: "=?iso-8859-1?Q?hyggem=F8de?= torsdag"
 * OUT: "hyggem\xC3\xB8de torsdag"
 */
char *unistr_header_to_utf8(const char *str)
{
	char *my_str;
	char *word;
	char *p;
	unistr *us;
	char *ret;

	my_str = mystrdup(str);
	us = unistr_new();

	word = strtok_r(my_str, " \t\n", &p);
	while (word) {
		header_decode_word(word, us);
		word = strtok_r(NULL, " \t\n", &p);
		if (word)
			unistr_append_char(us, ' ');
	}

	myfree(my_str);

	ret = unistr_to_utf8(us);
	unistr_free(us);

	return ret;
}


static int is_ok_in_header(char ch)
{
	if ((ch >= 'a') && (ch <= 'z')) return 1;
	if ((ch >= 'A') && (ch <= 'Z')) return 1;
	if ((ch >= '0') && (ch <= '9')) return 1;
	if (ch == '.') return 1;
	if (ch == ',') return 1;
	if (ch == ':') return 1;
	if (ch == ';') return 1;
	if (ch == '-') return 1;
	if (ch == ' ') return 1;
	return 0;
}


/* IN: "hyggem\xC3\xB8de torsdag"
 * OUT: "=?utf-8?Q?hyggem=C3=B8de_torsdag?="
 */
char *unistr_utf8_to_header(const char *str)
{
	unistr *us;
	char *ret;
	const char *p;
	int clean;
	char buf[4];

	/* clean header? */
	clean = 1;
	for (p=str; *p; p++) {
		if (!is_ok_in_header(*p)) {
			clean = 0;
			break;
		}
	}
	if (clean) {
		return mystrdup(str);
	}

	us = unistr_new();

	unistr_append_usascii(us, "=?utf-8?q?", 10);
	for (p=str; *p; p++) {
		if (*p == 0x20) {
			unistr_append_char(us, '_');
		} else if (is_ok_in_header(*p)) {
			unistr_append_char(us, *p);
		} else {
			snprintf(buf, sizeof(buf), "=%02X", (unsigned char)*p);
			unistr_append_usascii(us, buf, 3);
		}
	}
	unistr_append_usascii(us, "?=", 2);

	ret = unistr_to_utf8(us);
	unistr_free(us);

	return ret;
}


/* IN: "hyggem\\u00F8de torsdag"
 * OUT: "hyggem\xC3\xB8de torsdag"
 */
char *unistr_escaped_to_utf8(const char *str)
{
	unistr_char ch;
	unistr *us;
	char *ret;
	char u[5];
	int len;
	int skip = 0;

	us = unistr_new();

	while (*str) {
		if (*str == '\\') {
			str++;
			if (*str == 'u' && !skip) {
				str++;
				if (!isxdigit(str[0]) ||
						!isxdigit(str[1]) ||
						!isxdigit(str[2]) ||
						!isxdigit(str[3])) {
					unistr_append_char(us, '?');
					continue;
				}
				u[0] = *str++;
				u[1] = *str++;
				u[2] = *str++;
				u[3] = *str++;
				u[4] = '\0';
				ch = strtol(u, NULL, 16);
				unistr_append_char(us, ch);
				continue;
			} else {
				unistr_append_char(us, '\\');
				/* Avoid processing the second backslash of a
				 * double-backslash; but if this was a such a
				 * one, go back to normal */
				skip = !skip;
				continue;
			}
		} else {
			u[0] = *str;
			len = 1;
			str++;
			while (*str && (unsigned char)u[0] > 0x7F) {
				u[0] = *str;
				len++;
				str++;
			}
			unistr_append_utf8(us, str - len, len);
		}
	}

	ret = unistr_to_utf8(us);
	unistr_free(us);

	return ret;
}
