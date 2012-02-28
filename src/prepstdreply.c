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
#include "statctrl.h"
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


struct conditional;
typedef struct conditional conditional;
struct conditional {
	int satisfied;
	int elsepart;
	conditional *outer;
};


enum conditional_target {
	ACTION,
	REASON,
	TYPE,
	CONTROL
};


enum wrap_mode {
	WRAP_WORD,
	WRAP_CHAR,
	WRAP_USER
};


struct text {
	char *action;
	char *reason;
	char *type;
	source *src;
	substitution *substs;
	char *mailname;
	formatted *fmts;
	int wrapindent;
	int wrapwidth;
	enum wrap_mode wrapmode;
	conditional *cond;
	conditional *skip;
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
	/* It is important for this function to leave the length of the
	 * processed portion unchanged, or increase it by just one ASCII
	 * character (for $$). */
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
	txt->action = NULL;
	txt->reason = NULL;
	txt->type = NULL;
	txt->substs = NULL;
	txt->mailname = NULL;
	txt->fmts = NULL;
	txt->wrapindent = 0;
	txt->wrapwidth = 0;
	txt->wrapmode = WRAP_WORD;
	txt->cond = NULL;
	txt->skip = NULL;

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

	txt->action = action != NULL ? mystrdup(action) : NULL;
	txt->reason = reason != NULL ? mystrdup(reason) : NULL;
	txt->type = type != NULL ? mystrdup(type) : NULL;

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
		const char *filename, int transparent) {
	char *line = *line_p;
	char *pos = *pos_p;
	char *tmp, *esc;
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
	src->transparent = transparent;
	src->limit = -1;
	txt->src = src;
	tmp = mygetline(fd);
	if (tmp == NULL) {
		close_source(txt);
		**pos_p = '\0';
		return;
	}
	if (!transparent) {
		esc = unistr_escaped_to_utf8(tmp);
		myfree(tmp);
		tmp = esc;
	}
	line = concatstr(2, line, tmp);
	*pos_p = line + (*pos_p - *line_p);
	myfree(*line_p);
	*line_p = line;
	myfree(tmp);
}


static void begin_new_formatted_source(text *txt, char **line_p, char **pos_p,
		char *suffix, formatted *fmt, int transparent) {
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
	src->transparent = transparent;
	src->limit = -1;
	txt->src = src;
	str = (*fmt->get)(fmt->state);
	if (str == NULL) {
		close_source(txt);
		**line_p = '\0';
		*pos_p = *line_p;
		return;
	}
	if (!transparent) str = unistr_escaped_to_utf8(str);
	line = concatstr(2, line, str);
	/* The suffix will be added back in get_processed_text_line() */
	*pos_p = line + strlen(line);
	myfree(*line_p);
	*line_p = line;
}


static int handle_conditional(text *txt, char **line_p, char **pos_p,
		int *skipwhite_p, char *token,
		int neg, enum conditional_target tgt, int multi,
		const char *listdir)
{
	char *line = *line_p;
	char *pos;
	int satisfied = 0;
	int matches;
	conditional *cond;

	if (txt->skip == NULL) {
		pos = token;
		for (;;) {
			if (*token == '\0') break;
			for (; *pos != '\0' && (!multi || *pos != ' ');
					pos++) {
				if(*pos >= '0' && *pos <= '9') continue;
				if(*pos >= 'A' && *pos <= 'Z') continue;
				if(*pos >= 'a' && *pos <= 'z') continue;
				if(*pos == '_') continue;
				if(*pos == '-') continue;
				if(*pos == '.') continue;
				break;
			}
			if (*pos == ' ') {
				*pos = '\0';
			} else {
				multi = 0;
			}
			if (*pos != '\0') return 1;

			matches = 0;
			if (tgt == ACTION) {
				if (txt->action == NULL) return 1;
				if (strcasecmp(token, txt->action) == 0)
						matches = 1;
			} else if (tgt == REASON) {
				if (txt->reason == NULL) return 1;
				if (strcasecmp(token, txt->reason) == 0)
						matches = 1;
			} else if (tgt == TYPE) {
				if (txt->type == NULL) return 1;
				if (strcasecmp(token, txt->type) == 0)
						matches = 1;
			} else if (tgt == CONTROL) {
				if (statctrl(listdir, token))
						matches = 1;
			}
			if ((matches && !neg) || (!matches && neg)) {
				satisfied = 1;
				break;
			}

			if (!multi) break;
			*pos = ' ';
			pos++;
		}
	} else {
		/* We consider nested conditionals as successful while skipping
		 * text so they don't register themselves as the reason for
		 * skipping, nor trigger swallowing blank lines */
		satisfied = 1;
		pos = token + 1;
		while (*pos != '\0') pos++;
		multi = 0;
	}

	cond = mymalloc(sizeof(conditional));
	cond->satisfied = satisfied;
	cond->elsepart = 0;
	cond->outer = txt->cond;
	txt->cond = cond;
	if (!satisfied) txt->skip = cond;

	if (multi) {
		*pos = ' ';
		pos++;
		while (*pos != '\0') pos++;
	}
	pos++;
	if (*skipwhite_p) {
		while (*pos == ' ' || *pos == '\t') pos++;
	}
	line = concatstr(2, line, pos);
	*pos_p = line + (*pos_p - *line_p);
	myfree(*line_p);
	*line_p = line;

	return 0;
}


static int handle_directive(text *txt, char **line_p, char **pos_p,
		int *skipwhite_p, int conditionalsonly, const char *listdir) {
	/* This function returns 1 to swallow a preceding blank line, i.e. if
	 * we just finished processing a failed conditional without an else
	 * part, -1 if we did nothing due to only processing conditionals, and
	 * 0 otherwise. */
	char *line = *line_p;
	char *pos = *pos_p;
	char *token = pos + 1;
	char *endpos;
	char *filename;
	int limit;
	formatted *fmt;
	conditional *cond;
	int swallow;

	endpos = strchr(token, '%');
	if (endpos == NULL) {
		if (conditionalsonly) return -1;
		(*pos_p)++;
		return 0;
	}

	*pos = '\0';
	*endpos = '\0';

	if(strncmp(token, "ifaction ", 9) == 0) {
		token += 9;
		if (handle_conditional(txt, line_p, pos_p, skipwhite_p, token,
				0, ACTION, 1, listdir) == 0) return 0;
	} else if(strncmp(token, "ifreason ", 9) == 0) {
		token += 9;
		if (handle_conditional(txt, line_p, pos_p, skipwhite_p, token,
				0, REASON, 1, listdir) == 0) return 0;
	} else if(strncmp(token, "iftype ", 7) == 0) {
		token += 7;
		if (handle_conditional(txt, line_p, pos_p, skipwhite_p, token,
				0, TYPE, 1, listdir) == 0) return 0;
	} else if(strncmp(token, "ifcontrol ", 10) == 0) {
		token += 10;
		if (handle_conditional(txt, line_p, pos_p, skipwhite_p, token,
				0, CONTROL, 1, listdir) == 0) return 0;
	} else if(strncmp(token, "ifnaction ", 10) == 0) {
		token += 10;
		if (handle_conditional(txt, line_p, pos_p, skipwhite_p, token,
				1, ACTION, 0, listdir) == 0) return 0;
	} else if(strncmp(token, "ifnreason ", 10) == 0) {
		token += 10;
		if (handle_conditional(txt, line_p, pos_p, skipwhite_p, token,
				1, REASON, 0, listdir) == 0) return 0;
	} else if(strncmp(token, "ifntype ", 8) == 0) {
		token += 8;
		if (handle_conditional(txt, line_p, pos_p, skipwhite_p, token,
				1, TYPE, 0, listdir) == 0) return 0;
	} else if(strncmp(token, "ifncontrol ", 11) == 0) {
		token += 11;
		if (handle_conditional(txt, line_p, pos_p, skipwhite_p, token,
				1, CONTROL, 1, listdir) == 0) return 0;
	} else if(strcmp(token, "else") == 0) {
		if (txt->cond != NULL) {
			if (txt->skip == txt->cond) txt->skip = NULL;
			else if (txt->skip == NULL) txt->skip = txt->cond;
			txt->cond->elsepart = 1;
			endpos++;
			if (*skipwhite_p) {
				while (*endpos == ' ' || *endpos == '\t')
						endpos++;
			}
			line = concatstr(2, line, endpos);
			*pos_p = line + (*pos_p - *line_p);
			myfree(*line_p);
			*line_p = line;
			return 0;
		}
	} else if(strcmp(token, "endif") == 0) {
		if (txt->cond != NULL) {
			if (txt->skip == txt->cond) txt->skip = NULL;
			cond = txt->cond;
			swallow = (!cond->satisfied && !cond->elsepart)?1:0;
			txt->cond = cond->outer;
			myfree(cond);
			endpos++;
			if (*skipwhite_p) {
				while (*endpos == ' ' || *endpos == '\t')
						endpos++;
			}
			line = concatstr(2, line, endpos);
			*pos_p = line + (*pos_p - *line_p);
			myfree(*line_p);
			*line_p = line;
			return swallow;
		}
	}

	if (conditionalsonly) {
		*pos = '%';
		*endpos = '%';
		return -1;
	}

	if (txt->skip != NULL) {
		/* We don't process anything but conditionals if we're
		 * already skipping text in one. */
		*pos = '%';
		*endpos = '%';
		(*pos_p)++;
		return 0;
	}

	*skipwhite_p = 0;

	if(strcmp(token, "") == 0) {
		line = concatstr(3, line, "%", endpos + 1);
		*pos_p = line + (*pos_p - *line_p) + 1;
		myfree(*line_p);
		*line_p = line;
		return 0;
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
		return 0;
	} else if(strcmp(token, "comment") == 0 || strcmp(token, "$") == 0 ) {
		pos = endpos + 1;
		while (*pos != '\0' && *pos != '\r' && *pos != '\n') pos++;
		line = concatstr(2, line, pos);
		*pos_p = line + (*pos_p - *line_p);
		myfree(*line_p);
		*line_p = line;
		return 0;
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
			return 0;
		}
	} else if(strcmp(token, "ww") == 0 ||
			strcmp(token, "wordwrap") == 0 ||
			strcmp(token, "cw") == 0 ||
			strcmp(token, "charwrap") == 0 ||
			strcmp(token, "uw") == 0 ||
			strcmp(token, "userwrap") == 0) {
		if (*token == 'w') txt->wrapmode = WRAP_WORD;
		if (*token == 'c') txt->wrapmode = WRAP_CHAR;
		if (*token == 'u') txt->wrapmode = WRAP_USER;
		line = concatstr(2, line, endpos + 1);
		*pos_p = line + (*pos_p - *line_p);
		myfree(*line_p);
		*line_p = line;
		return 0;
	} else if(strncmp(token, "control ", 8) == 0) {
		token = filename_token(token + 8);
		if (token != NULL) {
			filename = concatstr(3, listdir, "/control/", token);
			begin_new_source_file(txt, line_p, pos_p, filename, 0);
			myfree(filename);
			return 0;
		}
	} else if(strncmp(token, "text ", 5) == 0) {
		token = filename_token(token + 5);
		if (token != NULL) {
			filename = concatstr(3, listdir, "/text/", token);
			begin_new_source_file(txt, line_p, pos_p, filename, 0);
			myfree(filename);
			return 0;
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
					txt->mailname, 1);
			if (limit == -1) txt->src->limit = -1;
			else txt->src->limit = limit - 1;
			return 0;
		}
	}
	if (token == NULL) {
		/* We have encountered a directive, but not been able to deal
		 * with it, so just advance through the string. */
		*pos = '%';
		*endpos = '%';
		(*pos_p)++;
		return 0;
	}

	fmt = txt->fmts;
	while (fmt != NULL) {
		if (strcmp(token, fmt->token) == 0) {
			begin_new_formatted_source(txt, line_p, pos_p,
					endpos + 1, fmt, 0);
			return 0;
		}
		fmt = fmt->next;
	}

	/* No recognised directive; just advance through the string. */
	*pos = '%';
	*endpos = '%';
	(*pos_p)++;
	return 0;
}


char *get_processed_text_line(text *txt, int headers,
		const char *listaddr, const char *listdelim,
		const char *listdir)
{
	char *line;
	const char *item;
	char *pos;
	char *tmp;
	char *prev = NULL;
	int len, i;
	int incision, spc;
	int directive, inhibitbreak;
	int peeking = 0; /* for a failed conditional without an else */
	int skipwhite; /* skip whitespace after a conditional directive */
	int swallow;

	for (;;) {
		line = NULL;
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
					tmp = mygetline(txt->src->fd);
				} else if (txt->src->fmt != NULL) {
					item = (*txt->src->fmt->get)(
						txt->src->fmt->state);
					if (item==NULL) tmp = NULL;
					else tmp = mystrdup(item);
				} else {
					tmp = NULL;
				}
				if (txt->src->limit > 0) txt->src->limit--;
				if (tmp == NULL) {
					txt->src->upcoming = NULL;
				} else if (txt->src->transparent) {
					txt->src->upcoming = tmp;
				} else {
					txt->src->upcoming =
						unistr_escaped_to_utf8(tmp);
					myfree(tmp);
				}
			} else {
				txt->src->upcoming = NULL;
			}
			if (txt->src->upcoming != NULL) continue;
			close_source(txt);
		}
		if (line == NULL) {
			if (peeking) return mystrdup("");
			if (prev != NULL) return prev;
			return NULL;
		}

		if (prev != NULL) {
			/* Wrapping */
			len = strlen(prev);
			pos = prev + len - 1;
			if (txt->wrapmode == WRAP_WORD) {
				while (pos > prev &&
						(*pos == ' ' || *pos == '\t'))
						pos--;
			}
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
				skipwhite = 0;
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
					if (txt->wrapmode == WRAP_WORD) {
					    tmp = concatstr(3, prev, " ", pos);
					    len++;
					} else {
					    tmp = concatstr(2, prev, pos);
					}
				}
				myfree(line);
				line = tmp;
				myfree(prev);
				pos = line + len;
				skipwhite = 1;
			}
			/* We can always line-break where the input had one */
			spc = len - 1;
			prev = NULL;
		} else {
			len = 0;
			pos = line;
			spc = -1;
			skipwhite = 0;
		}

		if (txt->skip != NULL) {
			incision = len;
		} else {
			incision = -1;
		}
		directive = 0;
		inhibitbreak = 0;
		while (*pos != '\0') {
			if (txt->wrapwidth != 0 && len >= txt->wrapwidth &&
					!peeking && spc != -1) break;
			if ((unsigned char)*pos > 0xbf && txt->skip == NULL &&
					txt->wrapmode == WRAP_CHAR &&
					!inhibitbreak) spc = len - 1;
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
				if (txt->skip == NULL &&
						txt->wrapmode != WRAP_USER &&
						!inhibitbreak) spc = len;
				inhibitbreak = 0;
			} else if (*pos == '\t') {
				/* Avoid breaking due to peeking */
				inhibitbreak = 0;
			} else if (txt->src->transparent) {
				/* Do nothing if the file is to be included
			 	 * transparently */
				if (peeking && txt->skip == NULL) break;
				inhibitbreak = 0;
			} else if (*pos == '\\' && txt->skip == NULL) {
				if (peeking) break;
				if (*(pos + 1) == '/') {
					spc = len - 1;
					tmp = pos + 2;
					inhibitbreak = 0;
				} else if (*(pos + 1) == '=') {
					tmp = pos + 2;
					/* Ensure we don't wrap the next
					 * character */
					inhibitbreak = 1;
				} else {
					/* Includes space and backslash */
					tmp = pos + 1;
					/* Ensure we don't wrap a space */
					if (*(pos+1) == ' ') inhibitbreak = 1;
					else inhibitbreak = 0;
				}
				*pos = '\0';
				tmp = concatstr(2, line, tmp);
				pos = tmp + len;
				myfree(line);
				line = tmp;
				skipwhite = 0;
				continue;
			} else if (*pos == '$' && txt->skip == NULL) {
				if (peeking) break;
				substitute_one(&line, &pos, listaddr,
						listdelim, listdir, txt);
				if (len != pos - line) {
					/* Cancel any break inhibition if the
					 * length changed (which will be
					 * because of $$) */
					inhibitbreak = 0;
					len = pos - line;
				}
				skipwhite = 0;
				/* The function sets up for the next character
				 * to process, so continue straight away. */
				continue;
			} else if (*pos == '%') {
				directive = 1;
				swallow = handle_directive(txt, &line, &pos,
						&skipwhite, peeking, listdir);
				if (swallow == 1) peeking = 0;
				if (swallow == -1) break;
				if (txt->skip != NULL) {
					if (incision == -1) {
						/* We have to cut a bit out
						 * later */
						incision = pos - line;
					}
				} else {
					if (incision != -1) {
					    /* Time to cut */
					    if (pos - line != incision) {
						line[incision] = '\0';
						tmp = concatstr(2, line, pos);
						pos = tmp + incision;
						myfree(line);
						line = tmp;
					    }
					    incision = -1;
					}
				}
				if (len != pos - line) {
					/* Cancel any break inhibition if the
					 * length changed (which will be
					 * because of %% or %^% or an empty
					 * list) */
					inhibitbreak = 0;
					len = pos - line;
				}
				/* handle_directive() sets up for the next
				 * character to process, so continue straight
				 * away. */
				continue;
			} else if (peeking && txt->skip == NULL) {
				break;
			}
			if (txt->skip == NULL) {
				len++;
			}
			pos++;
			skipwhite = 0;
		}

		if (incision == 0) {
			/* The whole line was skipped; nothing to return yet;
			 * keep reading */
			incision = -1;
			myfree(line);
			continue;
		}

		if (incision != -1) {
			/* Time to cut */
			if (pos - line != incision) {
				line[incision] = '\0';
				tmp = mystrdup(line);
				pos = tmp + incision;
				myfree(line);
				line = tmp;
			}
			incision = -1;
		}

		if (txt->wrapwidth != 0 && !peeking) {
			if (len < txt->wrapwidth) {
				prev = line;
				continue;
			}
			if (spc != -1) {
				if (txt->wrapmode == WRAP_WORD &&
					line[spc] == ' ') line[spc] = '\0';
				spc++;
				if (line[spc] == '\0') spc = -1;
			}
			if (spc != -1) {
				len = strlen(line + spc);
				if (txt->src->upcoming == NULL) {
				    tmp = mymalloc((len + txt->wrapindent + 1) *
					    sizeof(char));
				} else {
				    tmp = mymalloc((len + txt->wrapindent + 1 +
					    strlen(txt->src->upcoming)) *
					    sizeof(char));
				}
				pos = tmp;
				for (i = txt->wrapindent; i > 0; i--)
					*pos++ = ' ';
				strcpy(pos, line + spc);
				if (txt->src->upcoming != NULL) {
				    strcpy(pos + len, txt->src->upcoming);
				    myfree(txt->src->upcoming);
				}
				txt->src->upcoming = tmp;
			}
			line[spc] = '\0';
			tmp = mystrdup(line);
			myfree(line);
			line = tmp;
		} else {
			if (directive) {
				pos = line;
				while (*pos == ' ' || *pos == '\t') pos++;
				if (*pos == '\0') {
					/* Omit whitespace-only line with
					 * directives */
					myfree(line);
					continue;
				}
			}
			if (*line == '\0' && !headers && !peeking) {
				/* Non-wrapped bona fide blank line that isn't
				 * ending the headers; peek ahead to check it's
				 * not followed by an unsatisfied conditional
				 * without an else */
				peeking = 1;
				myfree(line);
				continue;
			} else if (peeking) {
				/* We found something; return preceding blank
				 * line */
				if (txt->src->upcoming == NULL) {
					txt->src->upcoming = line;
				} else {
					tmp = txt->src->upcoming;
					txt->src->upcoming = concatstr(3,
							line, "\n",
							txt->src->upcoming);
					myfree(line);
					myfree(tmp);
				}
				line = mystrdup("");
			}
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
	conditional *cond;
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
	while (txt->cond != NULL) {
		cond = txt->cond;
		txt->cond = txt->cond->outer;
		myfree(cond);
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
		line = get_processed_text_line(txt, 1, listaddr, listdelim,
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
		line = get_processed_text_line(txt, 0, listaddr, listdelim,
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
		line = get_processed_text_line(txt, 0, listaddr, listdelim,
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
