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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "prepstdreply.h"
#include "strgen.h"
#include "chomp.h"
#include "log_error.h"
#include "mygetline.h"
#include "wrappers.h"
#include "memory.h"
#include "getlistaddr.h"

char *substitute(const char *line, const char *listaddr, size_t datacount,
		 char **data)
{
	char *fqdn, *listname, *d1, *d2, *token, *value, *retstr;
	char *origline = mystrdup(line);
	size_t len, i;
	
	d1 = strchr(origline, '$');

	if(d1 == NULL)
		return origline;
	else
		d2 = strchr(d1 + 1, '$');
	
	if(d1 && d2) {
		len = d2 - d1;
		token = mymalloc(len + 1);
		snprintf(token, len, "%s", d1 + 1);
	} else
		return origline;

	*d1 = '\0';

	fqdn = genlistfqdn(listaddr);
	listname = genlistname(listaddr);

	if(strcmp(token, "listaddr") == 0) {
		value = mystrdup(listaddr);
		goto concatandreturn;
	} else if(strcmp(token, "listowner") == 0) {
		value = concatstr(3, listname, "+owner@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "helpaddr") == 0) {
		value = concatstr(3, listname, "+help@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "listgetN") == 0) {
		value = concatstr(3, listname, "+get-N@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "listunsubaddr") == 0) {
		value = concatstr(3, listname, "+unsubscribe@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "digestunsubaddr") == 0) {
		value = concatstr(3, listname, "+unsubscribe-digest@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "nomailunsubaddr") == 0) {
		value = concatstr(3, listname, "+unsubscribe-nomail@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "listsubaddr") == 0) {
		value = concatstr(3, listname, "+subscribe@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "digestsubaddr") == 0) {
		value = concatstr(3, listname, "+subscribe-digest@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "nomailsubaddr") == 0) {
		value = concatstr(3, listname, "+subscribe-nomail@", fqdn);
		goto concatandreturn;
	}
	if(data) {
		for(i = 0; i < datacount; i++) {
			if(strcmp(token, data[i*2]) == 0) {
				value = mystrdup(data[(i*2)+1]);
			}
		}
	}
concatandreturn:
	retstr = concatstr(3, origline, value, d2 + 1);
	myfree(origline);
	myfree(value);
	myfree(token);
	myfree(fqdn);
	myfree(listname);

	return retstr;
}

char *prepstdreply(const char *listdir, const char *filename, const char *from,
		   const char *to, const char *replyto, size_t tokencount,
		   char **data)
{
	int infd, outfd;
	char *listaddr, *myfrom, *str, *tmp, *subject, *retstr = NULL;
	char *myreplyto;

	tmp = concatstr(3, listdir, "/text/", filename);
	infd = open(tmp, O_RDONLY);
	myfree(tmp);
	if(infd < 0) {
		log_error(LOG_ARGS, "Could not open std mail %s", filename);
		return NULL;
	}

	listaddr = getlistaddr(listdir);

	tmp = mygetline(infd);
	if(strncasecmp(tmp, "Subject:", 8) != 0) {
		log_error(LOG_ARGS, "No Subject in listtexts. Using "
				"standard subject");
		subject = mystrdup("mlmmj administrativa");
	} else
		subject = substitute(tmp, listaddr, tokencount, data);

	myfree(tmp);
	
	myfrom = substitute(from, listaddr, tokencount, data);
	
	if(replyto)
		myreplyto = substitute(replyto, listaddr, tokencount, data);
	else
		myreplyto = NULL;
		
	do {
		tmp = random_str();
		myfree(retstr);
		retstr = concatstr(3, listdir, "/queue/", tmp);
		myfree(tmp);

		outfd = open(retstr, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);

	} while ((outfd < 0) && (errno == EEXIST));
	
	if(outfd < 0) {
		log_error(LOG_ARGS, "Could not open std mail %s", retstr);
		myfree(str);
		return NULL;
	}

	str = concatstr(4, myfrom, to, replyto, subject);

	if(writen(outfd, str, strlen(str)) < 0) {
		log_error(LOG_ARGS, "Could not write std mail");
		myfree(str);
		return NULL;
	}

	myfree(str);

	while((str = mygetline(infd))) {
		tmp = str;
		str = substitute(tmp, listaddr, tokencount, data);
		myfree(tmp);
		if(writen(outfd, str, strlen(str)) < 0) {
			myfree(str);
			log_error(LOG_ARGS, "Could not write std mail");
			return NULL;
		}
		myfree(str);
	}
	
	fsync(outfd);
	close(outfd);

	return retstr;
}
