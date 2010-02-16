/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 * Copyright (C) 2007 Morten K. Poulsen <morten at afdelingp.dk>
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
#include "mlmmj.h"
#include "getlistdelim.h"
#include "unistr.h"

char *substitute(const char *line, const char *listaddr, const char *listdelim,
		 size_t datacount, char **data, const char *mailname)
{
	char *s1, *s2;

	s1 = substitute_one(line, listaddr, listdelim, datacount, data, mailname);
	while(s1) {
		s2 = substitute_one(s1, listaddr, listdelim, datacount, data, mailname);
		if(s2) {
			myfree(s1);
			s1 = s2;
		} else
			return s1;
	}
		
	return mystrdup(line);
}

char *substitute_one(const char *line, const char *listaddr,
			const char *listdelim, size_t datacount, char **data,
			const char* mailname)
{
	char *fqdn, *listname, *d1, *d2, *token, *value = NULL;
	char *retstr, *origline;
	size_t len, i;

	if(line == NULL)
		return NULL;

	origline = mystrdup(line);

	d1 = strchr(origline, '$');

	if(d1 == NULL) {
		myfree(origline);
		return NULL;
	} else
		d2 = strchr(d1 + 1, '$');
	
	if(d1 && d2) {
		len = d2 - d1;
		token = mymalloc(len + 1);
		snprintf(token, len, "%s", d1 + 1);
	} else {
		myfree(origline);
		return NULL;
	}

	*d1 = '\0';

	fqdn = genlistfqdn(listaddr);
	listname = genlistname(listaddr);

	if(strcmp(token, "listaddr") == 0) {
		value = mystrdup(listaddr);
		goto concatandreturn;
	} else if(strcmp(token, "listowner") == 0) {
		value = concatstr(4, listname, listdelim, "owner@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "helpaddr") == 0) {
		value = concatstr(4, listname, listdelim, "help@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "faqaddr") == 0) {
		value = concatstr(4, listname, listdelim, "faq@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "listgetN") == 0) {
		value = concatstr(4, listname, listdelim, "get-N@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "listunsubaddr") == 0) {
		value = concatstr(4, listname, listdelim, "unsubscribe@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "digestunsubaddr") == 0) {
		value = concatstr(4, listname, listdelim,
				  "unsubscribe-digest@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "nomailunsubaddr") == 0) {
		value = concatstr(4, listname, listdelim,
				  "unsubscribe-nomail@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "listsubaddr") == 0) {
		value = concatstr(4, listname, listdelim, "subscribe@", fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "digestsubaddr") == 0) {
		value = concatstr(4, listname, listdelim, "subscribe-digest@",
				  fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "nomailsubaddr") == 0) {
		value = concatstr(4, listname, listdelim, "subscribe-nomail@",
				  fqdn);
		goto concatandreturn;
	} else if(strcmp(token, "originalmail") == 0) {
		/* append the first 100 lines of the mail inline */
		int mailfd;
		if(mailname && 
		     ((mailfd = open(mailname, O_RDONLY)) > 0)){
			size_t count = 0;
			char* str = NULL;
			while(count < 100 && (str = mygetline(mailfd))) {
				char* tmp = value;
				value = concatstr(3, value, " ", str);
				if(tmp)
					myfree(tmp);
				myfree(str);
				count++;
			}
			close(mailfd);
		}else{
			log_error(LOG_ARGS, "Could not substitute $originalmail$ (mailname == %s)",mailname);
		}
		goto concatandreturn;
	}
	if(data) {
		for(i = 0; i < datacount; i++) {
			if(strcmp(token, data[i*2]) == 0) {
				value = mystrdup(data[(i*2)+1]);
				goto concatandreturn;
			}
		}
	}

	myfree(origline);
	return NULL;

concatandreturn:
	retstr = concatstr(3, origline, value, d2 + 1);
	myfree(origline);
	myfree(value);
	myfree(token);
	myfree(fqdn);
	myfree(listname);

	return retstr;
}


int open_listtext(const char *listdir, const char *filename)
{
	char *tmp;
	int fd;

	tmp = concatstr(3, listdir, "/text/", filename);
	fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (fd >= 0)
		return fd;

	tmp = concatstr(2, DEFAULTTEXTDIR "/default/", filename);
	fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (fd >= 0)
		return fd;

	tmp = concatstr(2, DEFAULTTEXTDIR "/en/", filename);
	fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (fd >= 0)
		return fd;

	log_error(LOG_ARGS, "Could not open listtext '%s'", filename);
	return -1;
}


char *prepstdreply(const char *listdir, const char *filename, const char *from,
		   const char *to, const char *replyto, size_t tokencount,
		   char **data, char *customheaders, const char *mailname)
{
	int infd, outfd;
	char *listaddr, *listdelim, *myfrom, *tmp, *subject, *retstr = NULL;
	char *listfqdn, *line, *utfline, *utfsub, *utfsub2;
	char *myreplyto, *myto, *str = NULL, *mydate, *mymsgid;

	if ((infd = open_listtext(listdir, filename)) < 0) {
		return NULL;
	}

	listaddr = getlistaddr(listdir);
	listdelim = getlistdelim(listdir);
	listfqdn = genlistfqdn(listaddr);

	line = mygetline(infd);
	if(!line || (strncasecmp(line, "Subject: ", 9) != 0)) {
		log_error(LOG_ARGS, "No Subject in '%s' listtext. Using "
				"standard subject", filename);
		subject = mystrdup("mlmmj administrativa");
	} else {
		chomp(line);
		utfsub = unistr_escaped_to_utf8(line + 9);
		utfsub2 = substitute(utfsub, listaddr, listdelim, tokencount,
				     data, NULL);
		subject = unistr_utf8_to_header(utfsub2);
		myfree(utfsub);
		myfree(utfsub2);
		myfree(line);

		/* skip empty line after subject */
		line = mygetline(infd);
		if (line && (line[0] == '\n')) {
			myfree(line);
			line = NULL;
		}
	}
	if (line) {
		utfline = unistr_escaped_to_utf8(line);
		myfree(line);
	} else {
		utfline = NULL;
	}
	
	myfrom = substitute(from, listaddr, listdelim, tokencount, data, NULL);
	myto = substitute(to, listaddr, listdelim, tokencount, data, NULL);
	mydate = gendatestr();
	mymsgid = genmsgid(listfqdn);

	if(replyto) {
		myreplyto = substitute(replyto, listaddr, listdelim,
				       tokencount, data, NULL);
		tmp = concatstr(3, "Reply-To: ", myreplyto, "\n");
		myfree(myreplyto);
		myreplyto = tmp;
	} else
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
		myfree(listaddr);
		myfree(listdelim);
		myfree(listfqdn);
		myfree(utfline);
		return NULL;
	}

	str = concatstr(14,
			"From: ", myfrom,
			"\nTo: ", myto,
			"\n", myreplyto,
			mymsgid,
			mydate,
			"Subject: ", subject,
			"\nMIME-Version: 1.0"
			"\nContent-Type: text/plain; charset=utf-8"
			"\nContent-Encoding: 8bit"
			"\n", customheaders,
			"\n", utfline);

	myfree(utfline);

	if(writen(outfd, str, strlen(str)) < 0) {
		log_error(LOG_ARGS, "Could not write std mail");
		myfree(str);
		myfree(listaddr);
		myfree(listdelim);
		myfree(listfqdn);
		return NULL;
	}

	myfree(str);

	while((str = mygetline(infd))) {
		tmp = str;
		utfline = unistr_escaped_to_utf8(str);
		myfree(tmp);

		tmp = utfline;
		str = substitute(utfline, listaddr, listdelim, tokencount, data, mailname);
		myfree(tmp);

		if(writen(outfd, str, strlen(str)) < 0) {
			myfree(str);
			myfree(listaddr);
			myfree(listdelim);
			myfree(listfqdn);
			log_error(LOG_ARGS, "Could not write std mail");
			return NULL;
		}
		myfree(str);
	}
	
	fsync(outfd);
	close(outfd);

	myfree(listaddr);
	myfree(listdelim);
	myfree(listfqdn);

	return retstr;
}
