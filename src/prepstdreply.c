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


struct source;
typedef struct source source;
struct source {
	source *prev;
	char *upcoming;
	char *prefix;
	char *suffix;
	int fd;
	int transparent;
	int limit;
};


struct substitution;
typedef struct substitution substitution;
struct substitution {
	char *token;
	char *subst;
	substitution *next;
};


struct text {
	source *src;
	substitution *substs;
	char *mailname;
};


static char *filename_token(char *token) {
	char *pos;
	if (*token == '\0') return NULL;
	for(pos = token; *pos != '\0'; pos++) {
		if(*pos >= '0' && *pos <= '9') continue;
		if(*pos >= 'A' && *pos <= 'Z') continue;
		if(*pos >= 'a' && *pos <= 'z') continue;
		if(*pos == '_') continue;
		if(*pos == '-') continue;
		if(*pos == '.') continue;
		break;
	}
	if (*pos != '\0') return NULL;
	return token;
}


static char *numeric_token(char *token) {
	char *pos;
	if (*token == '\0') return NULL;
	for(pos = token; *pos != '\0'; pos++) {
		if(*pos >= '0' && *pos <= '9') continue;
		break;
	}
	if (*pos != '\0') return NULL;
	return token;
}


static void substitute_one(char **line_p, char **pos_p, const char *listaddr,
			const char *listdelim, const char *listdir, text *txt)
{
	char *line = *line_p;
	char *pos = *pos_p;
	char *token = pos + 1;
	char *endpos;
	char *fqdn, *listname;
	char *value = NULL;
	substitution *subst;

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
		token = filename_token(token + 8);
		if (token != NULL) value = ctrlcontent(listdir, token);
	} else if(strncmp(token, "text ", 5) == 0) {
		token = filename_token(token + 5);
		if (token != NULL) value = textcontent(listdir, token);
	} else if(strcmp(token, "originalmail") == 0) {
		/* DEPRECATED: use %originalmail% instead */
		value = mystrdup(" %originalmail 100%");
	} else {
		subst = txt->substs;
		while (subst != NULL) {
			if(strcmp(token, subst->token) == 0) {
				value = mystrdup(subst->subst);
				break;
			}
			subst = subst->next;
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
		const char *listdir, text *txt)
{
	char *new;
	char *pos;

	new = mystrdup(line);
	pos = new;

	while (*pos != '\0') {
		if (*pos == '$') {
			substitute_one(&new, &pos,
					listaddr, listdelim, listdir, txt);
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
	txt->src = mymalloc(sizeof(source));
	txt->src->prev = NULL;
	txt->src->upcoming = NULL;
	txt->src->prefix = NULL;
	txt->src->suffix = NULL;
	txt->src->transparent = 0;
	txt->src->limit = -1;
	txt->substs = NULL;
	txt->mailname = NULL;

	tmp = concatstr(3, listdir, "/text/", filename);
	txt->src->fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (txt->src->fd >= 0) return txt;

	tmp = concatstr(2, DEFAULTTEXTDIR "/default/", filename);
	txt->src->fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (txt->src->fd >= 0) return txt;

	tmp = concatstr(2, DEFAULTTEXTDIR "/en/", filename);
	txt->src->fd = open(tmp, O_RDONLY);
	myfree(tmp);
	if (txt->src->fd >= 0) return txt;

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


void register_unformatted(text *txt, const char *token, const char *replacement)
{
	substitution * subst = mymalloc(sizeof(substitution));
	subst->token = mystrdup(token);
	subst->subst = mystrdup(replacement);
	subst->next = txt->substs;
	txt->substs = subst;
}


void register_originalmail(text *txt, const char *mailname)
{
	txt->mailname = mystrdup(mailname);
}


static void begin_new_source_file(text *txt, char **line_p, char **pos_p,
		const char *filename) {
	char *line = *line_p;
	char *pos = *pos_p;
	char *tmp;
	source *src;
	int fd;
	size_t len;

	/* Save any later lines for use after finishing the source */
	while (*pos != '\0' && *pos != '\r' && *pos != '\n') pos++;
	if (*pos == '\r') pos++;
	if (*pos == '\n') pos++;
	if (*pos != '\0') txt->src->upcoming = mystrdup(pos);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		/* Act as if the source were an empty line */
		**pos_p = '\0';
		return;
	}

	src = mymalloc(sizeof(source));
	src->prev = txt->src;
	src->upcoming = NULL;
	len = strlen(line);
	src->prefix = mymalloc((len + 1) * sizeof(char));
	for (tmp = src->prefix; len > 0; ++tmp, --len) *tmp = ' ';
	*tmp = '\0';
	src->suffix = NULL;
	src->fd = fd;
	src->transparent = 0;
	src->limit = -1;
	txt->src = src;
	tmp = mygetline(fd);
	line = concatstr(2, line, tmp);
	*pos_p = line + (*pos_p - *line_p);
	myfree(*line_p);
	*line_p = line;
	myfree(tmp);
}


static void handle_directive(text *txt, char **line_p, char **pos_p,
		const char *listdir) {
	char *line = *line_p;
	char *pos = *pos_p;
	char *token = pos + 1;
	char *endpos;
	char *filename;
	int limit;

	endpos = strchr(token, '%');
	if (endpos == NULL) {
		(*pos_p)++;
		return;
	}

	*pos = '\0';
	*endpos = '\0';

	if(strcmp(token, "") == 0) {
		line = concatstr(3, line, "%", endpos + 1);
		*pos_p = line + (*pos_p - *line_p) + 1;
		myfree(*line_p);
		*line_p = line;
		return;
	} else if(strcmp(token, "^") == 0) {
		if (txt->src->prefix != NULL) {
			line[strlen(txt->src->prefix)] = '\0';
			line = concatstr(2, line, endpos + 1);
		} else {
			line = mystrdup(endpos + 1);
		}
		*pos_p = line;
		myfree(*line_p);
		*line_p = line;
		return;
	} else if(strcmp(token, "comment") == 0 || strcmp(token, "$") == 0 ) {
		/* Skip the rest of the line; the earlier part which we
		 * will return has already been truncated; the caller
		 * will save the next line for later use if necessary. */
		pos = endpos + 1;
		while (*pos != '\0' && *pos != '\r' && *pos != '\n') pos++;
		*pos_p = pos;
		return;
	} else if(strncmp(token, "control ", 8) == 0) {
		token = filename_token(token + 8);
		if (token != NULL) {
			filename = concatstr(3, listdir, "/control/", token);
			begin_new_source_file(txt, line_p, pos_p, filename);
			myfree(filename);
			return;
		}
	} else if(strncmp(token, "text ", 5) == 0) {
		token = filename_token(token + 5);
		if (token != NULL) {
			filename = concatstr(3, listdir, "/text/", token);
			begin_new_source_file(txt, line_p, pos_p, filename);
			myfree(filename);
			return;
		}
	} else if(strncmp(token, "originalmail", 12) == 0 &&
			txt->mailname != NULL) {
		token += 12;
		limit = 0;
		if (*token == '\0') {
			limit = -1;
		} else if (*token == ' ') {
			token = numeric_token(token + 1);
			if (token != NULL) limit = atol(token);
		} else {
			token = numeric_token(token);
			if (token != NULL) limit = atol(token);
		}
		if (limit != 0) {
			begin_new_source_file(txt, line_p, pos_p,
					txt->mailname);
			txt->src->transparent = 1;
			if (limit == -1) txt->src->limit = -1;
			else txt->src->limit = limit - 1;
			return;
		}
	}
	if (token == NULL) {
		/* We have encountered a directive, but not been able to deal
		 * with it, so just advance through the string. */
		*pos = '%';
		*endpos = '%';
		(*pos_p)++;
		return;
	}

	/* No recognised directive; just advance through the string. */
	*pos = '%';
	*endpos = '%';
	(*pos_p)++;
	return;
}


char *get_processed_text_line(text *txt,
		const char *listaddr, const char *listdelim,
		const char *listdir)
{
	char *line = NULL;
	char *pos;
	char *tmp;
	source *src;

	while (txt->src != NULL) {
		if (txt->src->upcoming != NULL) {
			if (txt->src->prefix != NULL) {
				line = concatstr(2, txt->src->prefix, txt->src->upcoming);
				myfree(txt->src->upcoming);
			} else {
				line = txt->src->upcoming;
			}
			txt->src->upcoming = NULL;
			break;
		}
		if (txt->src->limit != 0) {
			txt->src->upcoming = mygetline(txt->src->fd);
			if (txt->src->limit > 0) txt->src->limit--;
		} else {
			txt->src->upcoming = NULL;
		}
		if (txt->src->upcoming != NULL) continue;
		close(txt->src->fd);
		src = txt->src;
		txt->src = txt->src->prev;
		myfree(src);
	}
	if (line == NULL) return NULL;

	tmp = unistr_escaped_to_utf8(line);
	myfree(line);
	line = tmp;

	pos = line;
	while (*pos != '\0') {
		if (*pos == '\r') {
			*pos = '\0';
			pos++;
			if (*pos == '\n') pos++;
			if (*pos == '\0') break;
			txt->src->upcoming = mystrdup(pos);
			break;
		} else if (*pos == '\n') {
			*pos = '\0';
			pos++;
			if (*pos == '\0') break;
			txt->src->upcoming = mystrdup(pos);
			break;
		} else if (txt->src->transparent) {
			/* Do nothing if the file is to be included
			 * transparently */
		} else if (*pos == '$') {
			substitute_one(&line, &pos,
					listaddr, listdelim, listdir, txt);
			/* The function sets up for the next character
			 * to process, so continue straight away. */
			continue;
		} else if (*pos == '%') {
			handle_directive(txt, &line, &pos, listdir);
			/* The function sets up for the next character
			 * to process, so continue straight away. */
			continue;
		}
		pos++;
	}

	if (txt->src->suffix != NULL) {
		tmp = concatstr(2, line, txt->src->suffix);
		myfree(line);
		return tmp;
	} else {
		return line;
	}
}


void close_text(text *txt) {
	source *tmp;
	substitution *subst;
	while (txt->src != NULL) {
		close(txt->src->fd);
		tmp = txt->src;
		txt->src = txt->src->prev;
		myfree(tmp);
	}
	while (txt->substs != NULL) {
		subst = txt->substs;
		myfree(subst->token);
		myfree(subst->subst);
		txt->substs = txt->substs->next;
		myfree(subst);
	}
	if (txt->mailname != NULL) myfree(txt->mailname);
	myfree(txt);
}


char *prepstdreply(text *txt, const char *listdir,
		   const char *from, const char *to, const char *replyto)
{
	size_t len, i;
	int outfd;
	char *listaddr, *listdelim, *tmp, *retstr = NULL;
	char *listfqdn, *line;
	char *str;
	char *headers[10] = { NULL }; /* relies on NULL to flag end */

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

	for (i=0; i<6; i++) { 
		tmp = mystrdup("randomN");
		tmp[6] = '0' + i;
		str = random_str();
		register_unformatted(txt, tmp, str);
		myfree(tmp);
		myfree(str);
	}

	tmp = substitute(from, listaddr, listdelim, listdir, txt);
	headers[0] = concatstr(2, "From: ", tmp);
	myfree(tmp);
	tmp = substitute(to, listaddr, listdelim, listdir, txt);
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
		tmp = substitute(replyto, listaddr, listdelim, listdir, txt);
		headers[8] = concatstr(2, "Reply-To: ", tmp);
		myfree(tmp);
	}

	for(;;) {
		line = get_processed_text_line(txt, listaddr, listdelim,
				listdir);
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
				listdir);
	}
	while(line) {
			len = strlen(line);
			line[len] = '\n';
			if(writen(outfd, line, len+1) < 0) {
				myfree(str);
				log_error(LOG_ARGS, "Could not write std mail");
				myfree(retstr);
				retstr = NULL;
				goto freeandreturn;
			}
		myfree(line);
		line = get_processed_text_line(txt, listaddr, listdelim,
				listdir);
	}

	fsync(outfd);
	close(outfd);

freeandreturn:
	myfree(listaddr);
	myfree(listdelim);
	myfree(listfqdn);

	return retstr;
}
