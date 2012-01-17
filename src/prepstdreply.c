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


struct substitution;
typedef struct substitution substitution;
struct substitution {
	char *token;
	char *subst;
	substitution *next;
};


struct formatted;
typedef struct formatted formatted;
struct formatted {
	char *token;
	rewind_function rew;
	get_function get;
	void *state;
	formatted *next;
};


struct source;
typedef struct source source;
struct source {
	source *prev;
	char *upcoming;
	char *prefix;
	char *suffix;
	int fd;
	formatted *fmt;
	int transparent;
	int limit;
};


struct text {
	source *src;
	substitution *substs;
	char *mailname;
	formatted *fmts;
	size_t wrapindent;
	size_t wrapwidth;
};


struct memory_lines_state {
	char *lines;
	char *pos;
};


struct file_lines_state {
	char *filename;
	int fd;
	char truncate;
	char *line;
};


memory_lines_state *init_memory_lines(const char *lines)
{
	/* We use a static variable rather than dynamic allocation as
	 * there will never be two lists in use simultaneously */
	static memory_lines_state s;
	size_t len;

	/* Ensure there *is* a trailing newline */
	s.pos = NULL;
	len = strlen(lines);
	if (lines[len-1] == '\n') {
		s.lines = mystrdup(lines);
		return &s;
	}
	s.lines = mymalloc((len + 2) * sizeof(char));
	strcpy(s.lines, lines);
	s.lines[len] = '\n';
	s.lines[len+1] = '\0';
	return &s;
}


void rewind_memory_lines(void *state)
{
	memory_lines_state *s = (memory_lines_state *)state;
	if (s == NULL) return;
	s->pos = NULL;
}


const char *get_memory_line(void *state)
{
	memory_lines_state *s = (memory_lines_state *)state;
	char *line, *pos;

	if (s == NULL) return NULL;

	if (s->pos != NULL) *s->pos++ = '\n';
	else s->pos = s->lines;

	line = s->pos;
	pos = line;

	if (*pos == '\0') {
		s->pos = NULL;
		return NULL;
	}

	while (*pos != '\n') pos++;
	*pos = '\0';

	s->pos = pos;
	return line;
}


void finish_memory_lines(memory_lines_state *s)
{
	if (s == NULL) return;
	myfree(s->lines);
}


file_lines_state *init_file_lines(const char *filename, int open_now)
{
	/* We use a static variable rather than dynamic allocation as
	 * there will never be two lists in use simultaneously */
	static file_lines_state s;

	if (open_now) {
		s.fd = open(filename, O_RDONLY);
		s.filename = NULL;
		if (s.fd < 0) return NULL;
	} else {
		s.filename = mystrdup(filename);
		s.fd = -1;
	}

	s.truncate = '\0';
	s.line = NULL;
	return &s;
}


file_lines_state *init_truncated_file_lines(const char *filename, int open_now,
		char truncate)
{
	file_lines_state *s;
	s = init_file_lines(filename, open_now);
	if (s == NULL) return NULL;
	s->truncate = truncate;
	return s;
}


void rewind_file_lines(void *state)
{
	file_lines_state *s = (file_lines_state *)state;
	if (s == NULL) return;
	if (s->filename != NULL) {
		s->fd = open(s->filename, O_RDONLY);
		myfree(s->filename);
		s->filename = NULL;
	}
	if (s->fd >= 0) {
		if(lseek(s->fd, 0, SEEK_SET) < 0) {
			log_error(LOG_ARGS, "Could not seek to start of file");
			close(s->fd);
			s->fd = -1;
		}
	}
}


const char *get_file_line(void *state)
{
	file_lines_state *s = (file_lines_state *)state;
	char *end;
	if (s == NULL) return NULL;
	if (s->line != NULL) {
		myfree(s->line);
		s->line = NULL;
	}
	if (s->fd >= 0) {
		s->line = mygetline(s->fd);
		if (s->line == NULL) return NULL;
		if (s->truncate != '\0') {
			end = strchr(s->line, s->truncate);
			if (end == NULL) return NULL;
			*end = '\0';
		} else {
			chomp(s->line);
		}
		return s->line;
	}
	return NULL;
}


void finish_file_lines(file_lines_state *s)
{
	if (s == NULL) return;
	if (s->line != NULL) myfree(s->line);
	if (s->fd >= 0) close(s->fd);
	if (s->filename != NULL) myfree(s->filename);
}


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
	txt->src->fmt = NULL;
	txt->substs = NULL;
	txt->mailname = NULL;
	txt->fmts = NULL;
	txt->wrapindent = 0;
	txt->wrapwidth = 0;

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


void close_source(text *txt) {
	source *tmp;
	if (txt->src->fd != -1) close(txt->src->fd);
	if (txt->src->prefix != NULL) myfree(txt->src->prefix);
	if (txt->src->suffix != NULL) myfree(txt->src->suffix);
	tmp = txt->src;
	txt->src = txt->src->prev;
	myfree(tmp);
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


void register_formatted(text *txt, const char *token,
		rewind_function rew, get_function get, void *state)
{
	formatted * fmt = mymalloc(sizeof(formatted));
	fmt->token = mystrdup(token);
	fmt->rew = rew;
	fmt->get = get;
	fmt->state = state;
	fmt->next = txt->fmts;
	txt->fmts = fmt;
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
	src->fmt = NULL;
	src->transparent = 0;
	src->limit = -1;
	txt->src = src;
	tmp = mygetline(fd);
	if (tmp == NULL) {
		close_source(txt);
		**pos_p = '\0';
		return;
	}
	line = concatstr(2, line, tmp);
	*pos_p = line + (*pos_p - *line_p);
	myfree(*line_p);
	*line_p = line;
	myfree(tmp);
}


static void begin_new_formatted_source(text *txt, char **line_p, char **pos_p,
		char *suffix, formatted *fmt) {
	char *line = *line_p;
	char *pos = *pos_p;
	const char *str;
	source *src;

	/* Save any later lines for use after finishing the source */
	while (*pos != '\0' && *pos != '\r' && *pos != '\n') pos++;
	if (*pos == '\r') pos++;
	if (*pos == '\n') pos++;
	if (*pos != '\0') txt->src->upcoming = mystrdup(pos);

	(*fmt->rew)(fmt->state);

	src = mymalloc(sizeof(source));
	src->prev = txt->src;
	src->upcoming = NULL;
	if (*line == '\0') {
		src->prefix = NULL;
	} else {
		src->prefix = mystrdup(line);
	}
	if (*suffix == '\0' || *suffix == '\r' || *suffix == '\n') {
		src->suffix = NULL;
	} else {
		src->suffix = mystrdup(suffix);
	}
	src->fd = -1;
	src->fmt = fmt;
	src->transparent = 0;
	src->limit = -1;
	txt->src = src;
	str = (*fmt->get)(fmt->state);
	if (str == NULL) {
		close_source(txt);
		**line_p = '\0';
		*pos_p = *line_p;
		return;
	}
	line = concatstr(2, line, str);
	/* The suffix will be added back in get_processed_text_line() */
	*pos_p = line + strlen(line);
	myfree(*line_p);
	*line_p = line;
}


static void handle_directive(text *txt, char **line_p, char **pos_p,
		const char *listdir) {
	char *line = *line_p;
	char *pos = *pos_p;
	char *token = pos + 1;
	char *endpos;
	char *filename;
	int limit;
	formatted *fmt;

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
		pos = endpos + 1;
		while (*pos != '\0' && *pos != '\r' && *pos != '\n') pos++;
		line = concatstr(2, line, pos);
		*pos_p = line + (*pos_p - *line_p);
		myfree(*line_p);
		*line_p = line;
		return;
	} else if(strncmp(token, "wrap", 4) == 0) {
		token += 4;
		limit = 0;
		if (*token == '\0') {
			limit = 76;
		} else if (*token == ' ') {
			token = numeric_token(token + 1);
			if (token != NULL) limit = atol(token);
		}
		if (limit != 0) {
			txt->wrapindent = strlen(line);
			if (txt->src->prefix != NULL)
					txt->wrapindent -= strlen(txt->src->prefix);
			txt->wrapwidth = limit;
			line = concatstr(2, line, endpos + 1);
			*pos_p = line + (*pos_p - *line_p);
			myfree(*line_p);
			*line_p = line;
			return;
		}
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

	fmt = txt->fmts;
	while (fmt != NULL) {
		if (strcmp(token, fmt->token) == 0) {
			begin_new_formatted_source(txt, line_p, pos_p,
					endpos + 1, fmt);
			return;
		}
		fmt = fmt->next;
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
	const char *item;
	char *pos;
	char *tmp, *spc;
	char *prev = NULL;
	size_t len, i;

	for (;;) {
		while (txt->src != NULL) {
			if (txt->src->upcoming != NULL) {
				if (txt->src->prefix != NULL) {
					line = concatstr(2, txt->src->prefix,
							txt->src->upcoming);
					myfree(txt->src->upcoming);
				} else {
					line = txt->src->upcoming;
				}
				txt->src->upcoming = NULL;
				break;
			}
			if (txt->src->limit != 0) {
				if (txt->src->fd != -1) {
					txt->src->upcoming =
							mygetline(txt->src->fd);
				} else if (txt->src->fmt != NULL) {
					item = (*txt->src->fmt->get)(
							txt->src->fmt->state);
					if (item==NULL) txt->src->upcoming=NULL;
					else txt->src->upcoming=mystrdup(item);
				} else {
					txt->src->upcoming = NULL;
				}
				if (txt->src->limit > 0) txt->src->limit--;
			} else {
				txt->src->upcoming = NULL;
			}
			if (txt->src->upcoming != NULL) continue;
			close_source(txt);
		}
		if (line == NULL) return NULL;

		tmp = unistr_escaped_to_utf8(line);
		myfree(line);
		line = tmp;

		if (prev != NULL) {
			/* Wrapping */
			len = strlen(prev);
			pos = prev + len - 1;
			while (pos > prev && (*pos == ' ' || *pos == '\t'))
					pos--;
			pos++;
			*pos = '\0';
			len = pos - prev;
			if (*line == '\r' || *line == '\n' || *line == '\0') {
				/* Blank line; stop wrapping, finish
				   the last line and save the blank
				   line for later. */
				txt->wrapwidth = 0;
				txt->src->upcoming = line;
				line = prev;
				pos = line + len;
			} else {
				pos = line;
				while (*pos == ' ' || *pos == '\t') pos++;
				if (*pos == '\0') {
					myfree(line);
					continue;
				}
				if (*prev == '\0') {
					tmp = mystrdup(pos);
				} else {
					tmp = concatstr(3, prev, " ", pos);
				}
				myfree(line);
				line = tmp;
				myfree(prev);
				len++;
				pos = line + len;
			}
			prev = NULL;
		} else {
			len = 0;
			pos = line;
		}

		spc = NULL;
		while (*pos != '\0') {
			if (txt->wrapwidth != 0 && len > txt->wrapwidth) break;
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
			} else if (*pos == ' ') {
				spc = pos;
			} else if (txt->src->transparent) {
				/* Do nothing if the file is to be included
			 	 * transparently */
			} else if (*pos == '$') {
				substitute_one(&line, &pos, listaddr,
						listdelim, listdir, txt);
				len = pos - line;
				spc = NULL;
				/* The function sets up for the next character
				 * to process, so continue straight away. */
				continue;
			} else if (*pos == '%') {
				handle_directive(txt, &line, &pos, listdir);
				len = pos - line;
				spc = NULL;
				/* The function sets up for the next character
				 * to process, so continue straight away. */
				continue;
			}
			len++;
			pos++;
		}

		if (txt->wrapwidth != 0) {
			if (len <= txt->wrapwidth) {
				prev = line;
				continue;
			}
			if (spc == NULL) {
				pos = line + len - 1;
				while (pos >= line) {
					if (*pos == ' ') {
						spc = pos;
						break;
					}
					pos--;
				}
			}
			if (spc == NULL) {
				spc = line + txt->wrapwidth - 1;
			} else {
				*spc = '\0';
				spc++;
			}
			len = strlen(spc);
			if (txt->src->upcoming == NULL) {
				tmp = mymalloc((len + txt->wrapindent + 1) *
						sizeof(char));
			} else {
				tmp = mymalloc((len + txt->wrapindent + 1 +
						strlen(txt->src->upcoming)) *
						sizeof(char));
			}
			pos = tmp;
			for (i = txt->wrapindent; i > 0; i--) *pos++ = ' ';
			strcpy(pos, spc);
			if (txt->src->upcoming != NULL) {
				strcpy(pos + len, txt->src->upcoming);
				myfree(txt->src->upcoming);
			}
			txt->src->upcoming = tmp;
			*spc = '\0';
			tmp = mystrdup(line);
			myfree(line);
			line = tmp;
		}

		if (txt->src->suffix != NULL) {
			tmp = concatstr(2, line, txt->src->suffix);
			myfree(line);
			return tmp;
		} else {
			return line;
		}
	}
}


void close_text(text *txt) {
	substitution *subst;
	formatted *fmt;
	while (txt->src != NULL) {
		close_source(txt);
	}
	while (txt->substs != NULL) {
		subst = txt->substs;
		myfree(subst->token);
		myfree(subst->subst);
		txt->substs = txt->substs->next;
		myfree(subst);
	}
	if (txt->mailname != NULL) myfree(txt->mailname);
	while (txt->fmts != NULL) {
		fmt = txt->fmts;
		myfree(fmt->token);
		txt->fmts = txt->fmts->next;
		myfree(fmt);
	}
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
