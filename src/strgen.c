/* Copyright (C) 2003 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>
#include <libgen.h>

#include "strgen.h"
#include "wrappers.h"
#include "memory.h"

char *random_str()
{
	size_t len = 32;
	char *dest = mymalloc(len);

	snprintf(dest, len, "%X%X", random_int(), random_int());

	return dest;
}

char *random_plus_addr(const char *addr)
{
	size_t len = strlen(addr) + 16;
	char *dest = mymalloc(len);
	char *atsign;
	char *tmpstr;

	tmpstr = mymalloc(len);
	snprintf(tmpstr, len, "%s", addr);

	atsign = index(tmpstr, '@');
	*atsign = '=';

	snprintf(dest, len, "%d-%s", random_int(), tmpstr);

	myfree(tmpstr);
	
	return dest;
}

char *headerstr(const char *headertoken, const char *str)
{
	size_t len = strlen(headertoken) + strlen(str) + 2;
	char *dest = mymalloc(len);

	snprintf(dest, len, "%s%s\n", headertoken, str);

	return dest;
}

char *genlistname(const char *listaddr)
{
	size_t len;
	char *dest, *atsign;

	atsign = index(listaddr, '@');
	len = atsign - listaddr + 1;
	dest = mymalloc(len);
	
	snprintf(dest, len, "%s", listaddr);

	return dest;
}

char *genlistfqdn(const char *listaddr)
{
	size_t len;
	char *dest, *atsign;

	atsign = index(listaddr, '@');
	len = strlen(listaddr) - (atsign - listaddr);
	dest = mymalloc(len);
	snprintf(dest, len, "%s", atsign + 1);

	return dest;
}

char *concatstr(int count, ...)
{
	va_list arg;
        const char *str;
        int i;
        size_t len = 0;
        char *retstr;

        va_start(arg, count);

        for(i = 0; i < count; i++) {
                str = va_arg(arg, const char *);
                if(str)
                        len += strlen(str);
        }

        retstr = mymalloc(len + 1);
        retstr[0] = retstr[len] = 0;

        va_start(arg, count);

        for(i = 0; i < count; i++) {
                str = va_arg(arg, const char *);
                if(str)
                        strcat(retstr, str);
        }

        va_end(arg);

        return retstr;
}

char *hostnamestr()
{
        struct hostent *hostlookup;
        char hostname[1024];

        /* TODO use dynamic allocation */
        gethostname(hostname, sizeof(hostname) - 1);
        /* gethostname() is allowed to return an unterminated string */
        hostname[sizeof(hostname)-1] = '\0';
        hostlookup = gethostbyname(hostname);

        return mystrdup(hostlookup->h_name);
}

char *mydirname(const char *path)
{
	char *mypath, *dname, *ret;

	mypath = mystrdup(path);
	dname = dirname(mypath);
	ret = mystrdup(dname);

	/* We don't free mypath until we have strdup()'ed dname, because
	 * dirname() returns a pointer into mypath  -- mortenp 20040527 */
	myfree(mypath);

	return ret;
}

char *mybasename(const char *path)
{
	char *mypath, *bname, *ret;

	mypath = mystrdup(path);
	bname = basename(mypath);
	ret = mystrdup(bname);

	/* We don't free mypath until we have strdup()'ed bname, because
	 * basename() returns a pointer into mypath  -- mortenp 20040527 */
	myfree(mypath);
	
	return ret;
}

char *cleanquotedp(char *qpstr)
{
	char *retstr = mymalloc(strlen(qpstr));
	char qc[3], *c = qpstr;
	long qcval;
	int i = 0;

	/* XXX: We only use this function for checking whether the subject
	 * prefix is only present, so the recoding is neither guaranteed
	 * complete nor correct */

	qc[2] = '\0';
	while(*c != '\0') {
		switch(*c) {
			case '=':
				qc[0] = *(++c);
				qc[1] = *(++c);
				c++;
				qcval = strtol(qc, NULL, 16);
				if(qcval)
					retstr[i++] = (char)qcval;
				break;
			case '_':
				retstr[i++] = ' ';
				c++;
				break;
			default:
				retstr[i++] = *(c++);
				break;
		}
	}
	
	retstr[i] = '\0';

	return retstr;
}
