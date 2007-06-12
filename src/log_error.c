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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>

#include "log_error.h"
#include "config.h"
#include "memory.h"

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

char *log_name = NULL;

void log_set_name(const char* name)
{
	if (log_name) myfree(log_name);
	log_name = mystrdup(name);
}

void log_free_name()
{
	myfree(log_name);
}

void log_error(const char *file, int line, const char *errstr,
	const char *fmt, ...)
{
	static int syslog_is_open = 0;
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (!log_name) log_name = "mlmmj-UNKNOWN";

#ifdef HAVE_SYSLOG
	if(!syslog_is_open) {
		openlog(log_name, LOG_PID|LOG_CONS, LOG_MAIL);
		syslog_is_open = 1;
	}
	syslog(LOG_ERR, "%s:%d: %s: %s", file, line, buf, errstr);
#else
	fprintf(stderr, "%s[%d]: %s:%d: %s: %s\n", log_name, (int)getpid(),
			file, line, buf, errstr);
#endif
}
