/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "mlmmj.h"
#include "wrappers.h"
#include "memory.h"
#include "strgen.h"
#include "log_error.h"

static void print_help(const char *prg)
{
        printf("Usage: %s -L /path/to/listdir [-d] [-h] [-n] [-N]\n"
	       " -h: This help\n"
	       " -L: Full path to list directory\n"
	       " -d: Print for digesters list\n"
	       " -N: Print for nomail version of list\n"
	       " -n: Print subscriber count\n"
	       " -V: Print version\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int opt, fd, count = 0, docount = 0;
	char *listdir = NULL, *fileiter = NULL, *start;
	char *next, *cur, *subddir, *subdir;
	DIR *dirp;
	struct dirent *dp;
	struct stat st;
	size_t len;
	enum subtype typesub = SUB_NORMAL;

	while ((opt = getopt(argc, argv, "dhnNVL:")) != -1) {
		switch(opt) {
		case 'd':
			typesub = SUB_DIGEST;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'n':
			docount = 1;
			break;
		case 'N':
			typesub = SUB_NOMAIL;
			break;
		case 'V':
			print_version(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	if(listdir == NULL) {
		fprintf(stderr, "You have to specify -L\n"
				"%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	switch(typesub) {
		default:
		case SUB_NORMAL:
			subddir = mystrdup("/subscribers.d/");
			break;
		case SUB_DIGEST:
			subddir = mystrdup("/digesters.d/");
			break;
		case SUB_NOMAIL:
			subddir = mystrdup("/nomailsubs.d/");
			break;
	}
	
	subdir = concatstr(2, listdir, subddir);
	myfree(subddir);

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

		if(stat(fileiter, &st) < 0) {
			myfree(fileiter);
			continue;
		}
		if(!S_ISREG(st.st_mode)) {
			myfree(fileiter);
			continue;
		}
		if((fd = open(fileiter, O_RDONLY)) < 0) {
			myfree(fileiter);
			continue;
		}
		start = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
		if(start == MAP_FAILED) {
			myfree(fileiter);
			continue;
		}
		for(next = cur = start; next < start + st.st_size; next++) {
			if(*next == '\n' || next == start + st.st_size - 1) {
				len = next - cur;
				if(next == start + st.st_size - 1 &&
						*next != '\n')
					len++;
				if(docount)
					count++;
				else {
					writen(STDOUT_FILENO, cur, len);
					writen(STDOUT_FILENO, "\n", 1);
				}
				cur = next + 1;
			}
		}
		munmap(start, st.st_size);
		close(fd);
		myfree(fileiter);
	}

	if(docount)
		printf("%d\n", count);

	myfree(subdir);
	closedir(dirp);

	return 0;
}
