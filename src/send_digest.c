/* Copyright (C) 2004 Morten K. Poulsen <morten at afdelingp.dk>
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

#include "mlmmj.h"
#include "send_digest.h"
#include "log_error.h"
#include "strgen.h"
#include "memory.h"
#include "getlistaddr.h"
#include "wrappers.h"


int send_digest(const char *listdir, int firstindex, int lastindex,
		const char *addr, const char *mlmmjsend)
{
	int i, fd, archivefd, status, hdrfd;
	char buf[45];
	char *tmp, *queuename = NULL, *archivename, *fromstr;
	char *boundary, *listaddr, *listname, *listfqdn;
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
	myfree(listaddr);
	
	if (lastindex == firstindex) {
		snprintf(buf, sizeof(buf), " (%d)", firstindex);
	} else {
		snprintf(buf, sizeof(buf), " (%d-%d)", firstindex, lastindex);
	}

	fromstr = concatstr(4, "From: ", listname, "+help@", listfqdn);
	fromstr[6] = RECIPDELIM;
	tmp = concatstr(6, "\nMIME-Version: 1.0"
			    "\nContent-Type: multipart/" DIGESTMIMETYPE "; "
			    "boundary=", boundary,
			    "\nSubject: Digest of ", listname, buf, "\n\n");
	myfree(listfqdn);

	if (writen(fd, fromstr, strlen(fromstr)) < -1)
		goto errdighdrs;

	if(hdrfd >= 0 && dumpfd2fd(hdrfd, fd) < 0) {
		close(hdrfd);
		goto errdighdrs;
	}

	close(hdrfd);

	if (writen(fd, tmp, strlen(tmp)) < -1) {
errdighdrs:
		log_error(LOG_ARGS, "Could not write digest headers to '%s'",
				queuename);
		close(fd);
		unlink(queuename);
		myfree(boundary);
		myfree(fromstr);
		myfree(tmp);
		myfree(queuename);
		myfree(listname);
		return -1;
	}
	myfree(tmp);
	myfree(fromstr);

	for (i=firstindex; i<=lastindex; i++) {
		snprintf(buf, sizeof(buf), "%d", i);
		
		archivename = concatstr(3, listdir, "/archive/", buf);
		archivefd = open(archivename, O_RDONLY);
		myfree(archivename);
		
		if (archivefd < 0)
			continue;
		
		tmp = concatstr(7, "--", boundary,
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

	tmp = concatstr(3, "--", boundary, "--\n");
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
				NULL);
		log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
		exit(EXIT_FAILURE);  /* It is OK to exit, as this is a child */
	}

	unlink(queuename);
	myfree(queuename);

	return 0;
}
