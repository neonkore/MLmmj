/* Copyright (C) 2005 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "mlmmj.h"
#include "send_list.h"
#include "strgen.h"
#include "getlistaddr.h"
#include "getlistdelim.h"
#include "log_error.h"
#include "chomp.h"
#include "wrappers.h"
#include "mygetline.h"
#include "prepstdreply.h"
#include "memory.h"


struct subs_list_state;
typedef struct subs_list_state subs_list_state;
struct subs_list_state {
	char *dirname;
	DIR *dirp;
	int fd;
	char *line;
	int used;
};


static subs_list_state *init_subs_list(const char *dirname)
{
	subs_list_state *s = mymalloc(sizeof(subs_list_state));
	s->dirname = mystrdup(dirname);
	s->dirp = NULL;
	s->fd = -1;
	s->line = NULL;
	s->used = 0;
	return s;
}


static void rewind_subs_list(void *state)
{
	subs_list_state *s = (subs_list_state *)state;
	if (s == NULL) return;
	if (s->dirp != NULL) closedir(s->dirp);
	s->dirp = opendir(s->dirname);
	if(s->dirp == NULL) {
		log_error(LOG_ARGS, "Could not opendir(%s);\n", s->dirname);
	}
	s->used = 1;
}


static const char *get_sub(void *state)
{
	subs_list_state *s = (subs_list_state *)state;
	char *filename;
	struct dirent *dp;

	if (s == NULL) return NULL;
	if (s->dirp == NULL) return NULL;

	if (s->line != NULL) {
		myfree(s->line);
		s->line = NULL;
	}

	for (;;) {
		if (s->fd == -1) {
			dp = readdir(s->dirp);
			if (dp == NULL) {
				closedir(s->dirp);
				s->dirp = NULL;
				return NULL;
			}
			if ((strcmp(dp->d_name, "..") == 0) ||
					(strcmp(dp->d_name, ".") == 0))
					continue;
			filename = concatstr(2, s->dirname, dp->d_name);
			s->fd = open(filename, O_RDONLY);
			if(s->fd < 0) {
				log_error(LOG_ARGS,
						"Could not open %s for reading",
						filename);
				myfree(filename);
				continue;
			}
			myfree(filename);
		}
		s->line = mygetline(s->fd);
		if (s->line == NULL) {
			close(s->fd);
			s->fd = -1;
			continue;
		}
		chomp(s->line);
		return s->line;
	}
}


static void finish_subs_list(subs_list_state *s)
{
	if (s == NULL) return;
	if (s->line != NULL) myfree(s->line);
	if (s->fd != -1) close(s->fd);
	if (s->dirp != NULL) closedir(s->dirp);
	myfree(s->dirname);
	myfree(s);
}


static void print_subs(int fd, subs_list_state *s)
{
	const char *sub;
	rewind_subs_list(s);
	while ((sub = get_sub(s)) != NULL) {
		if (writen(fd, sub, strlen(sub)) < 0) {
			log_error(LOG_ARGS, "error writing subs list");
		}
		writen(fd, "\n", 1);
	}
}


void send_list(const char *listdir, const char *emailaddr,
	       const char *mlmmjsend)
{
	text *txt;
	subs_list_state *normalsls, *digestsls, *nomailsls;
	char *queuefilename, *listaddr, *listdelim, *listname, *listfqdn;
	char *fromaddr, *subdir, *nomaildir, *digestdir;
	int fd;

	listaddr = getlistaddr(listdir);
	listdelim = getlistdelim(listdir);
	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

	fromaddr = concatstr(4, listname, listdelim, "bounces-help@", listfqdn);
	myfree(listdelim);

	subdir = concatstr(2, listdir, "/subscribers.d/");
	digestdir = concatstr(2, listdir, "/digesters.d/");
	nomaildir = concatstr(2, listdir, "/nomailsubs.d/");
	normalsls = init_subs_list(subdir);
	digestsls = init_subs_list(digestdir);
	nomailsls = init_subs_list(nomaildir);
	myfree(subdir);
	myfree(digestdir);
	myfree(nomaildir);

	txt = open_text(listdir, "list", NULL, NULL, subtype_strs[SUB_ALL],
			"listsubs");
	MY_ASSERT(txt);
	register_formatted(txt, "listsubs",
			rewind_subs_list, get_sub, normalsls);
	register_formatted(txt, "normalsubs",
			rewind_subs_list, get_sub, normalsls);
	register_formatted(txt, "digestsubs",
			rewind_subs_list, get_sub, digestsls);
	register_formatted(txt, "nomailsubs",
			rewind_subs_list, get_sub, nomailsls);
	queuefilename = prepstdreply(txt, listdir, "$listowner$", emailaddr, NULL);
	MY_ASSERT(queuefilename);
	close_text(txt);

	/* DEPRECATED */
	/* Add lists manually if they weren't encountered in the list text */
	if (!normalsls->used && !digestsls->used && !nomailsls->used) {
		fd = open(queuefilename, O_WRONLY);
		if(fd < 0) {
			log_error(LOG_ARGS, "Could not open sub list mail");
			exit(EXIT_FAILURE);
		}
		if(lseek(fd, 0, SEEK_END) < 0) {
			log_error(LOG_ARGS, "Could not seek to end of file");
			exit(EXIT_FAILURE);
		}
		print_subs(fd, normalsls);
		writen(fd, "\n-- \n", 5);
		print_subs(fd, nomailsls);
		writen(fd, "\n-- \n", 5);
		print_subs(fd, digestsls);
		writen(fd, "\n-- \nend of output\n", 19);
		close(fd);
	}

	finish_subs_list(normalsls);
	finish_subs_list(digestsls);
	finish_subs_list(nomailsls);
	myfree(listaddr);
	myfree(listname);
	myfree(listfqdn);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-L", listdir,
				"-T", emailaddr,
				"-F", fromaddr,
				"-m", queuefilename, (char *)NULL);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}
