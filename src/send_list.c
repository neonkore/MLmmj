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

void send_list(const char *listdir, const char *emailaddr,
	       const char *mlmmjsend)
{
	char *queuefilename, *listaddr, *listdelim, *listname, *listfqdn;
	char *fromaddr, *subdir, *fileiter;
	DIR *dirp;
	struct dirent *dp;
	int fd, subfd;

	listaddr = getlistaddr(listdir);
	listdelim = getlistdelim(listdir);
	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);
	subdir = concatstr(2, listdir, "/subscribers.d/");

	fromaddr = concatstr(4, listname, listdelim, "bounces-help@", listfqdn);
	myfree(listdelim);

	queuefilename = prepstdreply(listdir, "listsubs", "$listowner$",
					emailaddr, NULL, 0, NULL, NULL, NULL);
	if(queuefilename == NULL) {
		log_error(LOG_ARGS, "Could not prepare sub list mail");
		exit(EXIT_FAILURE);
	}

	fd = open(queuefilename, O_WRONLY);
	if(fd < 0) {
		log_error(LOG_ARGS, "Could not open sub list mail");
		exit(EXIT_FAILURE);
	}

	if(lseek(fd, 0, SEEK_END) < 0) {
		log_error(LOG_ARGS, "Could not seek to send of file");
		exit(EXIT_FAILURE);
	}

	dirp = opendir(subdir);
	if(dirp == NULL) {
		fprintf(stderr, "Could not opendir(%s);\n", subdir);
		exit(EXIT_FAILURE);
	}
	while((dp = readdir(dirp)) != NULL) {
		if((strcmp(dp->d_name, "..") == 0) ||
		   (strcmp(dp->d_name, ".") == 0))
			continue;

		fileiter = concatstr(2, subdir, dp->d_name);
		subfd = open(fileiter, O_RDONLY);
		if(subfd < 0) {
			log_error(LOG_ARGS, "Could not open %s for reading",
					fileiter);
			myfree(fileiter);
			continue;
		}
		if(dumpfd2fd(subfd, fd) < 0)
			log_error(LOG_ARGS, "Error dumping subfile content "
					"to sub list mail");

		close(subfd);
		myfree(fileiter);
	}

	writen(fd, "\n-- \nend of output\n", 19);

	close(fd);
	closedir(dirp);
	
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
