/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
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

#ifndef MLMMJ_MAINTD_H
#define MLMMJ_MAINTD_H

#include <sys/types.h>

int delolder(const char *dirname, time_t than);
int clean_moderation(const char *listdir);
int clean_discarded(const char *listdir);
int resend_queue(const char *listdir, const char *mlmmjsend);
int resend_requeue(const char *listdir, const char *mlmmjsend);
int clean_nolongerbouncing(const char *listdir);
int probe_bouncers(const char *listdir, const char *mlmmjbounce);
int unsub_bouncers(const char *listdir, const char *mlmmjunsub);

/* I know the below is nasty, but it requires C99 to have multiple
 * argument macros, and this would then be the only thing needing
 * C99 -- Jun 09 2004, mmj
 */

#define WRITEMAINTLOG4( s1, s2, s3, s4 ) do { \
		logstr = concatstr( s1, s2, s3, s4 ) ;\
		writen(maintdlogfd, logstr, strlen(logstr)); \
		myfree(logstr); \
		} while (0);

#define WRITEMAINTLOG6( s1, s2, s3, s4, s5, s6 ) do { \
		logstr = concatstr( s1, s2, s3, s4, s5, s6 ) ;\
		writen(maintdlogfd, logstr, strlen(logstr)); \
		myfree(logstr); \
		} while (0);

#endif /* MLMMJ_MAINTD_H */
