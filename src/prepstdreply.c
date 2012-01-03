/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 * Copyright (C) 2007 Morten K. Poulsen <morten at afdelingp.dk>
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "prepstdreply.h"
#include "ctrlvalue.h"
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


struct text {
	int fd;
};


static char *alphanum_token(char *token) {
	char *pos;
	if (*token == '\0') return NULL;
	for(pos = token; *pos != '\0'; pos++) {
		if(*pos >= '0' && *pos <= '9') continue;
		if(*pos >= 'A' && *pos <= 'Z') continue;
		if(*pos >= 'a' && *pos <= 'z') continue;
		break;
	}
	if (*pos != '\0') return NULL;
	return token;
}


static void substitute_one(char **line_p, char **pos_p, const char *listaddr,
			const char *listdelim, size_t datacount, char **data,
			const char *listdir)
{
	char *line = *line_p;
	char *pos = *pos_p;
	char *token = pos + 1;
	char *endpos;
	char *fqdn, *listname;
	char *value = NULL;
	size_t i;

	endpos = strchr(token, '$');
	if (endpos == NULL) {
		(*pos_p)++;
		return;
	}

	*pos = '\0';
	*endpos = '\0';

	fqdn = genlistfqdn(listaddr);
	listname = genlistname(listaddr);

	if(strcmp(token, "") == 0) {
		value = mystrdup("$");
	} else if(strcmp(token, "listaddr") == 0) {
		/* DEPRECATED: use $list$@$domain$ instead */
		value = mystrdup(listaddr);
	} else if(strcmp(token, "list+") == 0) {
		value = concatstr(2, listname, listdelim);
	} else if(strcmp(token, "list") == 0) {
		value = mystrdup(listname);
	} else if(strcmp(token, "domain") == 0) {
		value = mystrdup(fqdn);
	} else if(strcmp(token, "listowner") == 0) {
		/* DEPRECATED: use $list+$owner@$domain$ instead */
		value = concatstr(4, listname, listdelim, "owner@", fqdn);
	} else if(strcmp(token, "helpaddr") == 0) {
		/* DEPRECATED: use $list+$help@$domain$ instead */
		value = concatstr(4, listname, listdelim, "help@", fqdn);
	} else if(strcmp(token, "faqaddr") == 0) {
		/* DEPRECATED: use $list+$faq@$domain$ instead */
		value = concatstr(4, listname, listdelim, "faq@", fqdn);
	} else if(strcmp(token, "listgetN") == 0) {
		/* DEPRECATED: use $list+$get-N@$domain$ instead */
		value = concatstr(4, listname, listdelim, "get-N@", fqdn);
	} else if(strcmp(token, "listunsubaddr") == 0) {
		/* DEPRECATED: use $list+$unsubscribe@$domain$ instead */
		value = concatstr(4, listname, listdelim, "unsubscribe@", fqdn);
	} else if(strcmp(token, "digestunsubaddr") == 0) {
		/* DEPRECATED: use $list+$unsubscribe-digest@$domain$ instead */
		value = concatstr(4, listname, listdelim,
				  "unsubscribe-digest@", fqdn);
	} else if(strcmp(token, "nomailunsubaddr") == 0) {
		/* DEPRECATED: use $list+$unsubscribe-nomail@$domain$ instead */
		value = concatstr(4, listname, listdelim,
				  "unsubscribe-nomail@", fqdn);
	} else if(strcmp(token, "listsubaddr") == 0) {
		/* DEPRECATED: use $list+$subscribe@$domain$ instead */
		value = concatstr(4, listname, listdelim, "subscribe@", fqdn);
	} else if(strcmp(token, "digestsubaddr") == 0) {
		/* DEPRECATED: use $list+$subscribe-digest@$domain$ instead */
		value = concatstr(4, listname, listdelim, "subscribe-digest@",
				  fqdn);
	} else if(strcmp(token, "nomailsubaddr") == 0) {
		/* DEPRECATED: use $list+$subscribe-nomail@$domain$ instead */
		value = concatstr(4, listname, listdelim, "subscribe-nomail@",
				  fqdn);
	} else if(strncmp(token, "control ", 8) == 0) {
		token = alphanum_token(token + 8);
		if (token != NULL) value = ctrlcontent(listdir, token);
	} else if(strncmp(token, "text ", 5) == 0) {
		token = alphanum_token(token + 5);
		if (token != NULL) value = textcontent(listdir, token);
	} else if(data) {
		for(i = 0; i < datacount; i++) {
			if(strcmp(token, data[i*2]) == 0) {
				value = mystrdup(data[(i*2)+1]);
				break;
			}
		}
	}

	if (value != NULL) {
		line = concatstr(3, line, value, endpos + 1);
		*pos_p = line + (*pos_p - *line_p);
		if (strcmp(value, "$") == 0) (*pos_p)++;
		myfree(*line_p);
		*line_p = line;
		myfree(value);
	} else {
		*pos = '$';
		*endpos = '$';
		(*pos_p)++;
	}
	myfree(fqdn);
	myfree(listname);
}


char *substitute(const char *line, const char *listaddr, const char *listdelim,
		 size_t datacount, char **data, const char *listdir)
{
	char *new;
	char *pos;

	new = mystrdup(line);
	pos = new;

	while (*pos != '\0') {
		if (*pos == '$') {
			substitute_one(&new, &pos,
					listaddr, listdelim,
					datacount, data, listdir);
			/* The function sets up for the next character
			 * to process, so continue straight away. */
			continue;
		}
		pos++;
	}

	return new;
}


text *open_text_file(const char *listdir, const char *filename)
{
	char *tmp;
	text *txt;

	txt = mymalloc(sizeof(text));

	tmp = concatstr(3, listdir, "/text/", filename);
	txt->fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (txt->fd >= 0) return txt;

	tmp = concatstr(2, DEFAULTTEXTDIR "/default/", filename);
	txt->fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (txt->fd >= 0) return txt;

	tmp = concatstr(2, DEFAULTTEXTDIR "/en/", filename);
	txt->fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (txt->fd >= 0) return txt;

	return NULL;
}


text *open_text(const char *listdir, const char *purpose, const char *action,
		   const char *reason, const char *type, const char *compat)
{
	size_t filenamelen, len;
	char *filename;
	text *txt;

	filename = concatstr(7,purpose,"-",action,"-",reason,"-",type);
	filenamelen = strlen(filename);
	do {
		if ((txt = open_text_file(listdir, filename)) != NULL) break;
		len = type ? strlen(type) : 0;
		filename[filenamelen-len-1] = '\0';
		if ((txt = open_text_file(listdir, filename)) != NULL) break;
		filename[filenamelen-len-1] = '-';
		filenamelen -= len + 1;
		len = reason ? strlen(reason) : 0;
		filename[filenamelen-len-1] = '\0';
		if ((txt = open_text_file(listdir, filename)) != NULL) break;
		filename[filenamelen-len-1] = '-';
		filenamelen -= len + 1;
		len = action ? strlen(action) : 0;
		filename[filenamelen-len-1] = '\0';
		if ((txt = open_text_file(listdir, filename)) != NULL) break;
		filename[filenamelen-len-1] = '-';
		filenamelen -= len + 1;
		if ((txt = open_text_file(listdir, compat)) != NULL) {
			myfree(filename);
			filename = mystrdup(compat);
			break;
		}
		log_error(LOG_ARGS, "Could not open listtext '%s'", filename);
		myfree(filename);
		return NULL;
	} while (0);

	return txt;
}


char *get_processed_text_line(text *txt,
		const char *listaddr, const char *listdelim,
		size_t datacount, char **data, const char *listdir)
{
	char *line;
	char *tmp;
	char *retstr;

	line = mygetline(txt->fd);
	if (line == NULL) return NULL;

	chomp(line);

	tmp = unistr_escaped_to_utf8(line);
	myfree(line);

	retstr = substitute(tmp, listaddr, listdelim,
	                 datacount, data, listdir);
	myfree(tmp);

	return retstr;
}


void close_text(text *txt) {
	close(txt->fd);
}


char *prepstdreply(const char *listdir, const char *purpose, const char *action,
		   const char *reason, const char *type, const char *compat,
		   const char *from, const char *to, const char *replyto,
		   size_t tokencount, char **data, const char *mailname)
{
	size_t len, i;
	int outfd, mailfd;
	text *txt;
	char *listaddr, *listdelim, *tmp, *retstr = NULL;
	char *listfqdn, *line;
	char *str;
	char **moredata;
	char *headers[10] = { NULL }; /* relies on NULL to flag end */

	txt = open_text(listdir, purpose, action, reason, type, compat);
	if (txt == NULL) return NULL;

	listaddr = getlistaddr(listdir);
	listdelim = getlistdelim(listdir);
	listfqdn = genlistfqdn(listaddr);

	do {
		tmp = random_str();
		if (retstr)
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
		myfree(retstr);
		myfree(txt);
		return NULL;
	}

	moredata = mymalloc(2*(tokencount+6) * sizeof(char *));
	for (i=0; i<2*tokencount; i++) {
		moredata[i] = data[i];
	}
	for (i=0; i<6; i++) { 
		moredata[2*(tokencount+i)] = mystrdup("randomN");
		moredata[2*(tokencount+i)][6] = '0' + i;
		moredata[2*(tokencount+i)+1] = random_str();
	}
	tokencount += 6;

	tmp = substitute(from, listaddr, listdelim,
	                 tokencount, moredata, listdir);
	headers[0] = concatstr(2, "From: ", tmp);
	myfree(tmp);
	tmp = substitute(to, listaddr, listdelim,
	                 tokencount, moredata, listdir);
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
		                 tokencount, moredata, listdir);
		headers[8] = concatstr(2, "Reply-To: ", tmp);
		myfree(tmp);
	}

	for(;;) {
		line = get_processed_text_line(txt, listaddr, listdelim,
				tokencount, moredata, listdir);
		if (!line) {
			log_error(LOG_ARGS, "No body in listtext");
			break;
		}
		if (*line == '\0') {
			/* end of headers */
			myfree(line);
			line = NULL;
			break;
		}
		if (*line == ' ' || *line == '\t') {
			/* line beginning with linear whitespace is a
			   continuation of previous header line */
			len = strlen(line);
			line[len] = '\n';
			if(writen(outfd, line, len+1) < 0) {
				log_error(LOG_ARGS, "Could not write std mail");
				myfree(line);
				myfree(retstr);
				retstr = NULL;
				goto freeandreturn;
			}
		} else {
			tmp = line;
			len = 0;
			while (*tmp && *tmp != ':') {
				tmp++;
				len++;
			}
			if (!*tmp) {
				log_error(LOG_ARGS, "No headers or invalid "
						"header in listtext");
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
			if (strncasecmp(line, "Subject:", len) == 0) {
				tmp = unistr_utf8_to_header(tmp);
				myfree(line);
				line = concatstr(2, "Subject:", tmp);
				myfree(tmp);
			}
			len = strlen(line);
			line[len] = '\n';
			if(writen(outfd, line, len+1) < 0) {
				log_error(LOG_ARGS, "Could not write std mail");
				myfree(line);
				myfree(retstr);
				retstr = NULL;
				goto freeandreturn;
			}
		}
		myfree(line);
		line = NULL;
	}

	for (i=0; headers[i] != NULL; i++) {
		len = strlen(headers[i]);
		headers[i][len] = '\n';
		if(writen(outfd, headers[i], len+1) < 0) {
			log_error(LOG_ARGS, "Could not write std mail");
			if (line)
				myfree(line);
			myfree(retstr);
			retstr = NULL;
			goto freeandreturn;
		}
	}

	/* end the headers */
	if(writen(outfd, "\n", 1) < 0) {
		log_error(LOG_ARGS, "Could not write std mail");
		if (line)
			myfree(line);
		myfree(retstr);
		retstr = NULL;
		goto freeandreturn;
	}

	if (line == NULL) {
		line = get_processed_text_line(txt, listaddr, listdelim,
				tokencount, moredata, listdir);
	}
	while(line) {
		tmp = line;
		while (*tmp && (*tmp == ' ' || *tmp == '\t')) {
			tmp++;
		}
		if (strncmp(tmp,"$originalmail",13) == 0) {
			*tmp = '\0';
			tmp += 13;
			str = tmp;
			if (*tmp == ' ') {
				tmp++;
				str = tmp;
			}
			while (*tmp >= '0' && *tmp <= '9')
				tmp++;
			if (*tmp == '$') {
				*tmp = '\0';
				len = 100;
				if (str != tmp)
					len = atol(str);
				if (mailname && 
		     		   ((mailfd = open(mailname, O_RDONLY)) >= 0)){
		     		    str = NULL;
				    i = 0;
				    while (i < len &&
				           (str = mygetline(mailfd))) {
				        tmp = str;
				        str = concatstr(2,line,str);
				        myfree(tmp);
				        if(writen(outfd,str,strlen(str)) < 0) {
				            myfree(str);
				            log_error(LOG_ARGS, "Could not write std mail");
					    myfree(retstr);
					    retstr = NULL;
					    goto freeandreturn;
				        }
				        myfree(str);
				        i++;
				    }
				    close(mailfd);
				} else {
				    log_error(LOG_ARGS, "Could not substitute $originalmail %d$ (mailname == %s)",len,mailname);
				}
			} else {
				log_error(LOG_ARGS, "Bad $originalmail N$ substitution");
			}
		} else {
			len = strlen(line);
			line[len] = '\n';
			if(writen(outfd, line, len+1) < 0) {
				myfree(str);
				log_error(LOG_ARGS, "Could not write std mail");
				myfree(retstr);
				retstr = NULL;
				goto freeandreturn;
			}
		}

		myfree(line);
		line = get_processed_text_line(txt, listaddr, listdelim,
				tokencount, moredata, listdir);
	}

	fsync(outfd);
	close(outfd);

freeandreturn:
	myfree(listaddr);
	myfree(listdelim);
	myfree(listfqdn);

	for (i=tokencount-6; i<tokencount; i++) {
		myfree(moredata[2*i]);
		myfree(moredata[2*i+1]);
	}
	myfree(moredata);

	close_text(txt);
	myfree(txt);

	return retstr;
}
