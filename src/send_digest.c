/* Copyright (C) 2004, 2005 Morten K. Poulsen <morten at afdelingp.dk>
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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>

#include "mlmmj.h"
#include "send_digest.h"
#include "log_error.h"
#include "strgen.h"
#include "memory.h"
#include "getlistaddr.h"
#include "getlistdelim.h"
#include "wrappers.h"
#include "prepstdreply.h"
#include "mygetline.h"
#include "gethdrline.h"
#include "statctrl.h"
#include "unistr.h"
#include "chomp.h"


struct mail {
	int idx;
	char *from;
};

struct thread {
	char *subject;
	int num_mails;
	struct mail *mails;
};


static char *thread_list(const char *listdir, int firstindex, int lastindex)
{
	int i, j, archivefd, thread_idx;
	char *ret, *line, *tmp, *subj, *from;
	char *archivename;
	int num_threads = 0;
	struct thread *threads = NULL;
	char buf[45];

	for (i=firstindex; i<=lastindex; i++) {

		snprintf(buf, sizeof(buf), "%d", i);

		archivename = concatstr(3, listdir, "/archive/", buf);
		archivefd = open(archivename, O_RDONLY);
		myfree(archivename);

		if (archivefd < 0)
			continue;

		subj = NULL;
		from = NULL;

		while ((line = gethdrline(archivefd))) {
			if (strcmp(line, "\n") == 0) {
				myfree(line);
				break;
			}
			if (strncasecmp(line, "Subject: ", 9) == 0) {
				myfree(subj);
				subj = unistr_header_to_utf8(line + 9);
			}
			if (strncasecmp(line, "From: ", 6) == 0) {
				myfree(from);
				from = unistr_header_to_utf8(line + 6);
			}
			myfree(line);
		}

		if (!subj) {
			subj = mystrdup("no subject");
		}

		if (!from) {
			from = mystrdup("anonymous");
		}

		tmp = subj;
		for (;;) {
			if (isspace(*tmp)) {
				tmp++;
				continue;
			}
			if (strncasecmp(tmp, "Re:", 3) == 0) {
				tmp += 3;
				continue;
			}
			break;
		}
		/* tmp is now the clean subject */

		thread_idx = -1;
		for (j=0; j<num_threads; j++) {
			if (strcmp(subj, threads[j].subject) == 0) {
				thread_idx = j;
				break;
			}
		}
		if (thread_idx == -1) {
			num_threads++;
			threads = myrealloc(threads,
					num_threads*sizeof(struct thread));
			threads[num_threads-1].subject = mystrdup(tmp);
			threads[num_threads-1].num_mails = 0;
			threads[num_threads-1].mails = NULL;
			thread_idx = num_threads-1;
		}

		threads[thread_idx].num_mails++;
		threads[thread_idx].mails = myrealloc(threads[thread_idx].mails,
				threads[thread_idx].num_mails*sizeof(struct mail));
		threads[thread_idx].mails[threads[thread_idx].num_mails-1].idx = i;
		threads[thread_idx].mails[threads[thread_idx].num_mails-1].from =
				concatstr(5, "      ", buf, " - ", from, "\n");

		myfree(subj);
		myfree(from);

		close(archivefd);
	}

	ret = mystrdup("");

	for (i=0; i<num_threads; i++) {

		tmp = concatstr(3, ret, threads[i].subject, "\n");
		myfree(ret);
		ret = tmp;
		myfree(threads[i].subject);

		for (j=0; j<threads[i].num_mails; j++) {
			tmp = concatstr(2, ret, threads[i].mails[j].from);
			myfree(ret);
			ret = tmp;
			myfree(threads[i].mails[j].from);
		}
		myfree(threads[i].mails);

		tmp = concatstr(2, ret, "\n");
		myfree(ret);
		ret = tmp;
	}
	myfree(threads);

	return ret;
}


int send_digest(const char *listdir, int firstindex, int lastindex,
		int issue, const char *addr, const char *mlmmjsend)
{
	int i, fd, archivefd, status, hdrfd;
	text * txt;
	char buf[45];
	char *tmp, *queuename = NULL, *archivename, *subject, *line = NULL;
	char *utfsub, *utfsub2, *utfline;
	char *boundary, *listaddr, *listdelim, *listname, *listfqdn;
	char *subst_data[10];
	pid_t childpid, pid;

	if (addr) {
		errno = 0;
		log_error(LOG_ARGS, "send_digest() does not support sending "
				"digest mails to only one recipient yet");
		return -1;
	}

	if (firstindex > lastindex)
		return -1;
	
	do {
		tmp = random_str();
		myfree(queuename);
		queuename = concatstr(3, listdir, "/queue/", tmp);
		myfree(tmp);
		fd = open(queuename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	} while ((fd < 0) && (errno == EEXIST));

	if (fd < 0) {
		log_error(LOG_ARGS, "Could not open digest queue file '%s'",
				queuename);
		myfree(queuename);
		return -1;
	}

	tmp = concatstr(2, listdir, "/control/customheaders");
	hdrfd = open(tmp, O_RDONLY);
	myfree(tmp);

	boundary = random_str();

	listaddr = getlistaddr(listdir);
	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	listdelim = getlistdelim(listdir);
	
	txt = open_text_file(listdir, "digest");
	if (txt == NULL) {
		log_error(LOG_ARGS, "Could not open listtext 'digest'");
	}

	subst_data[0] = "digestfirst";
	snprintf(buf, sizeof(buf), "%d", firstindex);
	subst_data[1] = mystrdup(buf);

	subst_data[2] = "digestlast";
	snprintf(buf, sizeof(buf), "%d", lastindex);
	subst_data[3] = mystrdup(buf);

	subst_data[4] = "digestinterval";
	if (lastindex == firstindex) {
		snprintf(buf, sizeof(buf), "%d", firstindex);
	} else {
		snprintf(buf, sizeof(buf), "%d-%d", firstindex, lastindex);
	}
	subst_data[5] = mystrdup(buf);

	subst_data[6] = "digestissue";
	snprintf(buf, sizeof(buf), "%d", issue);
	subst_data[7] = mystrdup(buf);

	subst_data[8] = "digestthreads";
	subst_data[9] = thread_list(listdir, firstindex, lastindex);

	if (txt == NULL || (line = get_text_line(txt)) == NULL ||
			(strncasecmp(line, "Subject: ", 9) != 0)) {

		utfsub = mystrdup("Digest of $listaddr$ issue $digestissue$"
				" ($digestinterval$)");
	} else {

		chomp(line);
		utfsub = unistr_escaped_to_utf8(line + 9);
	}

	utfsub2 = substitute(utfsub, listaddr, listdelim, 5, subst_data, listdir);
	subject = unistr_utf8_to_header(utfsub2);
	myfree(utfsub);
	myfree(utfsub2);

	tmp = concatstr(10, "From: ", listname, listdelim, "help@", listfqdn,
			   "\nMIME-Version: 1.0"
			   "\nContent-Type: multipart/" DIGESTMIMETYPE "; "
			   "boundary=", boundary,
			   "\nSubject: ", subject,
			   "\n");

	myfree(listfqdn);
	myfree(subject);

	if (writen(fd, tmp, strlen(tmp)) < 0) {
		myfree(tmp);
		goto errdighdrs;
	}
	myfree(tmp);

	if(hdrfd >= 0 && dumpfd2fd(hdrfd, fd) < 0) {
		goto errdighdrs;
	}

	close(hdrfd);
	hdrfd = -1;

	if (writen(fd, "\n", 1) < 0) {
errdighdrs:
		log_error(LOG_ARGS, "Could not write digest headers to '%s'",
				queuename);
		close(fd);
		unlink(queuename);
		myfree(boundary);
		myfree(queuename);
		myfree(listaddr);
		myfree(listname);
		myfree(listdelim);
		myfree(subst_data[1]);
		myfree(subst_data[3]);
		myfree(subst_data[5]);
		myfree(subst_data[7]);
		myfree(subst_data[9]);
		if (txt != NULL) {
			close_text(txt);
			myfree(line);
		}
		if (hdrfd > 0) {
			close(hdrfd);
		}
		return -1;
	}

	if ((txt != NULL) && !statctrl(listdir, "nodigesttext")) {

		tmp = concatstr(3, "\n--", boundary,
				"\nContent-Type: text/plain; charset=UTF-8"
				"\nContent-Transfer-Encoding: 8bit"
				"\n\n");
		if (writen(fd, tmp, strlen(tmp)) == -1) {
			log_error(LOG_ARGS, "Could not write digest text/plain"
					" part headers to '%s'", queuename);
			close(fd);
			unlink(queuename);
			myfree(boundary);
			myfree(tmp);
			myfree(queuename);
			myfree(listaddr);
			myfree(listname);
			myfree(listdelim);
			myfree(subst_data[1]);
			myfree(subst_data[3]);
			myfree(subst_data[5]);
			myfree(subst_data[7]);
			myfree(subst_data[9]);
			if (txt != NULL) {
				close_text(txt);
				myfree(line);
			}
			return -1;
		}
		myfree(tmp);

		if (line && (strncasecmp(line, "Subject: ", 9) == 0)) {
			myfree(line);
			line = get_text_line(txt);
			if (line && (strcmp(line, "\n") == 0)) {
				/* skip empty line after Subject: */
				line[0] = '\0';
			}
		}

		if (line) {
			do {
				utfline = unistr_escaped_to_utf8(line);
				myfree(line);

				tmp = substitute(utfline, listaddr, listdelim,
						5, subst_data, listdir);
				myfree(utfline);

				if(writen(fd, tmp, strlen(tmp)) < 0) {
					myfree(tmp);
					log_error(LOG_ARGS, "Could not write"
							" std mail");
					break;
				}
				myfree(tmp);
			} while ((line = get_text_line(txt)));
		}

		close_text(txt);
	} else if (txt != NULL) {
		close_text(txt);
	}

	myfree(line);
	myfree(listaddr);
	myfree(listdelim);
	myfree(subst_data[1]);
	myfree(subst_data[3]);
	myfree(subst_data[5]);
	myfree(subst_data[7]);
	myfree(subst_data[9]);

	for (i=firstindex; i<=lastindex; i++) {
		snprintf(buf, sizeof(buf), "%d", i);
		
		archivename = concatstr(3, listdir, "/archive/", buf);
		archivefd = open(archivename, O_RDONLY);
		myfree(archivename);
		
		if (archivefd < 0)
			continue;
		
		tmp = concatstr(7, "\n--", boundary,
				"\nContent-Type: message/rfc822"
				"\nContent-Disposition: inline; filename=\"",
					listname, "_", buf, ".eml\""
				"\n\n");
		if (writen(fd, tmp, strlen(tmp)) == -1) {
			log_error(LOG_ARGS, "Could not write digest part "
					"headers for archive index %d to "
					"'%s'", i, queuename);
			close(fd);
			close(archivefd);
			unlink(queuename);
			myfree(boundary);
			myfree(tmp);
			myfree(queuename);
			myfree(listname);
			return -1;
		}
		myfree(tmp);

		if (dumpfd2fd(archivefd, fd) < 0) {
			log_error(LOG_ARGS, "Could not write digest part %d "
					"to '%s'", i,
					queuename);
			close(fd);
			close(archivefd);
			unlink(queuename);
			myfree(boundary);
			myfree(queuename);
			myfree(listname);
			return -1;
		}
		
		close(archivefd);
	}

	tmp = concatstr(3, "\n--", boundary, "--\n");
	if (writen(fd, tmp, strlen(tmp)) == -1) {
		log_error(LOG_ARGS, "Could not write digest end to '%s'",
				queuename);
		close(fd);
		unlink(queuename);
		myfree(boundary);
		myfree(queuename);
		myfree(listname);
		return -1;
	}

	close(fd);
	myfree(boundary);
	myfree(listname);
	myfree(tmp);

	childpid = fork();

	if(childpid < 0) {
		log_error(LOG_ARGS, "Could not fork");
		myfree(queuename);
		return -1;
	}

	if(childpid > 0) {
		do /* Parent waits for the child */
		      pid = waitpid(childpid, &status, 0);
		while(pid == -1 && errno == EINTR);
	} else {
		execlp(mlmmjsend, mlmmjsend,
				"-l", "7",
				"-L", listdir,
				"-m", queuename,
				(char *)NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
		exit(EXIT_FAILURE);  /* It is OK to exit, as this is a child */
	}

	myfree(queuename);

	return 0;
}
