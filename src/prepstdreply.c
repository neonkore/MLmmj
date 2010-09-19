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
		   char **data, const char *mailname)
{
	size_t i, len;
	int infd, outfd;
	char *listaddr, *listdelim, *tmp, *retstr = NULL;
	char *listfqdn, *line, *utfline, *utfsub, *utfsub2;
	char *str = NULL;
	char *headers[10] = { NULL }; /* relies on NULL to flag end */

	if ((infd = open_listtext(listdir, filename)) < 0) {
		return NULL;
	}

	listaddr = getlistaddr(listdir);
	listdelim = getlistdelim(listdir);
	listfqdn = genlistfqdn(listaddr);

	do {
		tmp = random_str();
		myfree(retstr);
		retstr = concatstr(3, listdir, "/queue/", tmp);
		myfree(tmp);

		outfd = open(retstr, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);

	} while ((outfd < 0) && (errno == EEXIST));
	
	if(outfd < 0) {
		log_error(LOG_ARGS, "Could not open std mail %s", retstr);
		myfree(listaddr);
		myfree(listdelim);
		myfree(listfqdn);
		return NULL;
	}

	tmp = substitute(from, listaddr, listdelim,
	                 tokencount, data, NULL);
	headers[0] = concatstr(2, "From: ", tmp);
	myfree(tmp);
	tmp = substitute(to, listaddr, listdelim,
	                 tokencount, data, NULL);
	headers[1] = concatstr(2, "To: ", tmp);
	myfree(tmp);
	headers[2] = genmsgid(listfqdn);
	chomp(headers[2]);
	headers[3] = gendatestr();
	chomp(headers[3]);
	headers[4] = mystrdup("Subject: mlmmj administrivia");
	headers[5] = mystrdup("MIME-Version: 1.0");
	headers[6] = mystrdup("Content-Type: text/plain; charset=utf-8");
	headers[7] = mystrdup("Content-Transfer-Encoding: 8bit");

	if(replyto) {
		tmp = substitute(replyto, listaddr, listdelim,
		                 tokencount, data, NULL);
		headers[8] = concatstr(2, "Reply-To: ", tmp);
		myfree(tmp);
	}

	for(;;) {
		line = mygetline(infd);
		if (!line) {
			log_error(LOG_ARGS, "No body in '%s' listtext",
					filename);
			break;
		}
		if (*line == '\n') {
			/* end of headers */
			myfree(line);
			line = NULL;
			break;
		}
		chomp(line);
		if (*line == ' ' || *line == '\t') {
			/* line beginning with linear whitespace is a
			   continuation of previous header line */
			utfsub = unistr_escaped_to_utf8(line);
			str = substitute(utfsub, listaddr, listdelim,
			                 tokencount, data, NULL);
			myfree(utfsub);
			len = strlen(str);
			str[len] = '\n';
			if(writen(outfd, str, len+1) < 0) {
				log_error(LOG_ARGS, "Could not write std mail");
				myfree(str);
				myfree(line);
				myfree(listaddr);
				myfree(listdelim);
				myfree(listfqdn);
				return NULL;
			}
			myfree(str);
		} else {
			tmp = line;
			len = 0;
			while (*tmp && *tmp != ':') {
				tmp++;
				len++;
			}
			if (!*tmp) {
				log_error(LOG_ARGS, "No headers or invalid "
						"header in '%s' listtext",
						filename);
				break;
			}
			tmp++;
			len++;
			/* remove the standard header if one matches */
			for (i=0; headers[i] != NULL; i++) {
				if (strncasecmp(line, headers[i], len) == 0) {
					myfree(headers[i]);
					while (headers[i] != NULL) {
						headers[i] = headers[i+1];
						i++;
					}
					break;
				}
			}
			utfsub = unistr_escaped_to_utf8(tmp);
			*tmp = '\0';
			utfsub2 = substitute(utfsub, listaddr, listdelim,
			                     tokencount, data, NULL);
			myfree(utfsub);
			if (strncasecmp(line, "Subject:", len) == 0) {
				tmp = unistr_utf8_to_header(utfsub2);
				myfree(utfsub2);
				str = concatstr(2, line, tmp);
				myfree(tmp);
			} else {
				str = concatstr(2, line, utfsub2);
				myfree(utfsub2);
			}
			len = strlen(str);
			str[len] = '\n';
			if(writen(outfd, str, len+1) < 0) {
				log_error(LOG_ARGS, "Could not write std mail");
				myfree(str);
				myfree(line);
				myfree(listaddr);
				myfree(listdelim);
				myfree(listfqdn);
				return NULL;
			}
			myfree(str);
		}
		myfree(line);
	}

	for (i=0; headers[i] != NULL; i++) {
		len = strlen(headers[i]);
		headers[i][len] = '\n';
		if(writen(outfd, headers[i], len+1) < 0) {
			log_error(LOG_ARGS, "Could not write std mail");
			if (line)
				myfree(line);
			myfree(str);
			myfree(listaddr);
			myfree(listdelim);
			myfree(listfqdn);
			return NULL;
		}
	}

	/* end the headers */
	if(writen(outfd, "\n", 1) < 0) {
		log_error(LOG_ARGS, "Could not write std mail");
		myfree(str);
		if (line)
			myfree(line);
		myfree(listaddr);
		myfree(listdelim);
		myfree(listfqdn);
		return NULL;
	}

	if (line) {
		str = concatstr(2, line, "\n");
		myfree(line);
	} else {
		str = mygetline(infd);
	}
	while(str) {
		utfline = unistr_escaped_to_utf8(str);
		myfree(str);

		str = substitute(utfline, listaddr, listdelim, tokencount, data, mailname);
		myfree(utfline);

		if(writen(outfd, str, strlen(str)) < 0) {
			myfree(str);
			myfree(listaddr);
			myfree(listdelim);
			myfree(listfqdn);
			log_error(LOG_ARGS, "Could not write std mail");
			return NULL;
		}
		myfree(str);
		str = mygetline(infd);
	}

	fsync(outfd);
	close(outfd);

	myfree(listaddr);
	myfree(listdelim);
	myfree(listfqdn);

	return retstr;
}
