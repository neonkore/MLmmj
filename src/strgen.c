/* Copyright (C) 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
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

char *random_str()
{
	size_t len = 16;
	char *dest = malloc(len);

	snprintf(dest, len, "%d", random_int());

	return dest;
}

char *random_plus_addr(const char *addr)
{
	size_t len = strlen(addr) + 16;
	char *dest = malloc(len);
	char *atsign;
	char *tmpstr;

	tmpstr = malloc(len);
	snprintf(tmpstr, len, "%s", addr);

	atsign = index(tmpstr, '@');
	*atsign = '=';

	snprintf(dest, len, "%d-%s", random_int(), tmpstr);

	free(tmpstr);
	
	return dest;
}

char *headerstr(const char *headertoken, const char *str)
{
	size_t len = strlen(headertoken) + strlen(str) + 2;
	char *dest = malloc(len);

	snprintf(dest, len, "%s%s\n", headertoken, str);

	return dest;
}

char *genlistname(const char *listaddr)
{
	size_t len;
	char *dest, *atsign;

	atsign = index(listaddr, '@');
	len = atsign - listaddr + 1;
	dest = malloc(len);
	
	snprintf(dest, len, "%s", listaddr);

	return dest;
}

char *genlistfqdn(const char *listaddr)
{
	size_t len;
	char *dest, *atsign;

	atsign = index(listaddr, '@');
	len = strlen(listaddr) - (atsign - listaddr);
	dest = malloc(len);
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

        retstr = malloc(len + 1);
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

        return strdup(hostlookup->h_name);
}

char *mydirname(const char *path)
{
	char *mypath, *dname, *ret;

	mypath = strdup(path);
	dname = dirname(mypath);
	ret = strdup(dname);

	/* We don't free mypath until we have strdup()'ed dname, because
	 * dirname() returns a pointer into mypath  -- mortenp 20040527 */
	free(mypath);

	return ret;
}

char *mybasename(const char *path)
{
	char *mypath, *bname, *ret;

	mypath = strdup(path);
	bname = basename(mypath);
	ret = strdup(bname);

	/* We don't free mypath until we have strdup()'ed bname, because
	 * basename() returns a pointer into mypath  -- mortenp 20040527 */
	free(mypath);
	
	return ret;
}
