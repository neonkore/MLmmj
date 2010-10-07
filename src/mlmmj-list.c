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
	printf("Usage: %s -L /path/to/listdir\n"
	       "       [-c] [-d] [-h] [-m] [-n] [-o] [-s] [-V]\n"
	       " -L: Full path to list directory\n"
	       " -c: Print subscriber count\n"
	       " -d: Print for digesters list\n"
	       " -h: This help\n"
	       " -m: Print moderators for list\n"
	       " -n: Print for nomail version of list\n"
	       " -o: Print owner(s) of list\n"
	       " -s: Print normal subscribers (default) \n"
	       " -V: Print version\n", prg);
	exit(EXIT_SUCCESS);
}

int dumpcount(const char *filename, int *count)
{
	int fd;
	char *start, *next, *cur;
	struct stat st;
	size_t len;
	
	if((fd = open(filename, O_RDONLY)) < 0)
		return -1;

	if(stat(filename, &st) < 0)
		return -1;
	
	if(!S_ISREG(st.st_mode))
		return -1;
	
	/* Nobody there */
	if(st.st_size == 0) {
		return 0;
	}

	start = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(start == MAP_FAILED)
		return -1;

	for(next = cur = start; next < start + st.st_size; next++) {
		if(*next == '\n' || next == start + st.st_size - 1) {
			len = next - cur;
			if(next == start + st.st_size - 1 && *next != '\n')
				len++;
			if(count)
				(*count)++;
			else {
				writen(STDOUT_FILENO, cur, len);
				writen(STDOUT_FILENO, "\n", 1);
			}
			cur = next + 1;
		}
	}
	munmap(start, st.st_size);
	close(fd);

	return 0;
}

int main(int argc, char **argv)
{
	int opt, ret, count = 0, docount = 0;
	char *listdir = NULL, *fileiter = NULL, *tmp;
	char *subddir, *subdir, *subfile = NULL;
	DIR *dirp;
	struct dirent *dp;
	enum subtype typesub = SUB_NORMAL;

	while ((opt = getopt(argc, argv, "cdhmnosVL:")) != -1) {
		switch(opt) {
		case 'c':
			docount = 1;
			break;
		case 'd':
			typesub = SUB_DIGEST;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'm':
			typesub = SUB_FILE;
			subfile = "/control/moderators";
			break;
		case 'n':
			typesub = SUB_NOMAIL;
			break;
		case 'o':
			typesub = SUB_FILE;
			subfile = "/control/owner";
			break;
		case 'V':
			print_version(argv[0]);
			exit(EXIT_SUCCESS);
		default:
		case 's':
			typesub = SUB_NORMAL;
			break;
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
			subddir = "/subscribers.d/";
			break;
		case SUB_DIGEST:
			subddir = "/digesters.d/";
			break;
		case SUB_NOMAIL:
			subddir = "/nomailsubs.d/";
			break;
		case SUB_FILE:
			subddir = NULL;
			break;
	}
	
	if(subddir)
		subdir = concatstr(2, listdir, subddir);
	else
		subdir = NULL;

	if(subdir) {
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

			if(docount)
				dumpcount(fileiter, &count);
			else
				ret = dumpcount(fileiter, NULL);

			myfree(fileiter);
		}
		myfree(subdir);
		closedir(dirp);
	} else {
		tmp = concatstr(2, listdir, subfile);
		if(docount)
			dumpcount(tmp, &count);
		else
			dumpcount(tmp, NULL);
	}

	if(docount)
		printf("%d\n", count);


	return 0;
}
