/* Copyright (C) 2007 Sascha Sommer <ssommer at suse.de>
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

/* a version of mlmmj-recieve that parses the mail on the fly and strips unwanted
   mime parts
   opens the files control/mimedeny and control/mimestrip for a list of mime
   types for body parts that should be denied or stripped.
   It adds an extra header X-ThisMailContainsUnwantedMimeParts: Y for mails that
   contain disallowed mimeparts and X-ThisMailContainsUnwantedMimeParts: N otherwise
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>



#include "mlmmj.h"
#include "mygetline.h"
#include "gethdrline.h"
#include "strgen.h"
#include "chomp.h"
#include "ctrlvalue.h"
#include "ctrlvalues.h"

#include "log_error.h"
#include "wrappers.h"
#include "memory.h"

#define UNWANTED_MIME_HDR "X-ThisMailContainsUnwantedMimeParts: N\n"

/* append a copy of a string to a string list */
static void append_strtolist(struct strlist *list, char* str) {
	list->count++;
	list->strs = myrealloc(list->strs,
                         sizeof(char *) * (list->count + 1));
	list->strs[list->count-1] = mystrdup(str);
	list->strs[list->count] = NULL;
}

/* free all strings in a strlist */
static void free_strlist(struct strlist *list) {
	if(!list)
		return;
	if(list->strs) {
		int i;
		for(i=0;i < list->count;i++)
			myfree(list->strs[i]);
		myfree(list->strs);
	}
	list->strs = NULL;
	list->count = 0;
}

static int findit(char *line, char **headers)
{
        int i = 0;
        size_t len;

	if (!headers)
		return 0;

        while(headers[i]) {
                len = strlen(headers[i]);
                if(strncasecmp(line, headers[i], len) == 0)
                        return 1;
                i++;
        }

        return 0;
}

/* extract mime_type and boundary from the Content-Type header
 * allocates a string for the mime_type if one is found
 * always allocates a boundarie (using "--" when none is found)
 * the caller needs to free the allocated strings
*/
static void extract_boundary(struct strlist *allhdrs, char** mime_type, char** boundary)
{
	int x;
	*boundary = NULL;
	*mime_type = NULL;
	for( x = 0 ; x < allhdrs->count ; x++ ){
		char* hdr = allhdrs->strs[x];
		if(hdr && !strncasecmp(hdr,"Content-Type:",13)){
			char* pos = hdr + 13;
			size_t len = 0;

			/* find the start of the mimetype */
			while(*pos && (*pos == ' ' || *pos == '\t'))
				++pos;

			if(*pos == '"'){                   /* handle quoted mime types */
				++pos;
				while(pos[len] && pos[len] != '"')
					++len;
			}else{
				while(pos[len] && pos[len] != ' ' && pos[len] != '\t' && pos[len] != ';')
					++len;
			}

			/* extract mime type if any */
			if(len){
				*mime_type = mymalloc(len+1);
				strncpy(*mime_type,pos,len);
				(*mime_type)[len] = '\0';
			}

			pos += len;
			len = 0;
			/* find start of the boundary info */
			while(*pos && strncasecmp(pos,"boundary=",9))
				++pos;
			if(*pos == '\0')         /* no boundary */
				break;

			pos += 9;
			if(*pos == '"'){         /* quoted boundary */
				++pos;
				while(pos[len] && pos[len] != '"')
					++len;
			}else{                  /* unquoted boundary */
				while(pos[len] && pos[len] != ' ' && pos[len] != '\t' && pos[len] != ';')
					++len;
			}

			/* extract boundary */
			*boundary = mymalloc(len + 3);
			strcpy(*boundary,"--");
			strncat(*boundary,pos,len);
			break;
		}
	}
}

/* read all mail headers and save them in a strlist
 * check what to do with parts that contain the given mime_type
 *return values
 * 0: ok
 * 1: strip
 * sets deny to 1 if the entire mail should be denied
 */
#define MIME_OK 0
#define MIME_STRIP 1
static int read_hdrs(int fd, struct strlist *allhdrs,struct strlist* delmime,struct strlist* denymime,int* deny,char** boundary) {
	int result = MIME_OK;
	char* mime_type = NULL;
	allhdrs->strs = NULL;
	allhdrs->count = 0;
	/* read headers */
	while(1) {
		char* line = mygetline(fd);
		if(!line)        /* end of file and also end of headers */
			break;

		/* end of headers */
		if(line[0] == '\n'){
			myfree(line);
			break;
		}
		if(!allhdrs->count || ((line[0] != '\t') && (line[0] != ' '))) /* first header line or no more unfolding */
			append_strtolist(allhdrs,line);
		else{
			char* tmp = concatstr(2, allhdrs->strs[allhdrs->count-1], line);
			myfree(allhdrs->strs[allhdrs->count-1]);
		 	allhdrs->strs[allhdrs->count-1] = tmp;
		}
		myfree(line);
	}
	extract_boundary(allhdrs,&mime_type,boundary);
	if(mime_type) {
		/* check if this part should be stripped */
		if(delmime && findit(mime_type, delmime->strs))
			result = MIME_STRIP;
		/* check if the mail should be denied */
		if(denymime && findit(mime_type, denymime->strs))
			*deny = 1;
		myfree(mime_type);
	}
	return result;
}

/* writes the mail headers if unwantedmime_hdrpos is not NULL an UNWANTED_MIME_HDR
 * is inserted and its position saved in unwantedmime_hdrpos
 * returns 0 on success
 */
static int write_hdrs(int outfd,struct strlist* hdrs,off_t* unwantedmime_hdrpos) {
	int i;
	for(i = 0; i < hdrs->count ; i++) {
		if(writen(outfd, hdrs->strs[i], strlen(hdrs->strs[i])) < 0){
			log_error(LOG_ARGS, "Error when dumping headers");
			return -1;
		}
	}

	/* if this is not the header of an embedded part add the header that will
	   indicate if the mail contains unwanted mime parts */
	if(unwantedmime_hdrpos) {
		if(writen(outfd, UNWANTED_MIME_HDR,strlen(UNWANTED_MIME_HDR)) < 0){
			log_error(LOG_ARGS, "Error writting unwanted mime header");
			return -1;
		}
		/* get the current position so that we can update the header later */
		*unwantedmime_hdrpos = lseek(outfd,0,SEEK_CUR);
		if(*unwantedmime_hdrpos < 2){
			log_error(LOG_ARGS, "Error getting file position");
			return -1;
		}
		*unwantedmime_hdrpos -= 2;
	}

	/* write a single line feed to terminate the header part */
	if(writen(outfd, "\n", 1) < 0) {
		log_error(LOG_ARGS,"Error writting end of hdrs.");
		return -1;
	}
	return 0;
}

/* set the unwanted mime_hdr to Y */
static int update_unwantedmime_hdr(int outfd,off_t unwantedmime_hdrpos) {
	/* seek to the header position */
	if(lseek(outfd,unwantedmime_hdrpos,SEEK_SET) < 0) {
		log_error(LOG_ARGS,"Error seeking to the unwantedmime_hdr");
		return -1;
	}

	/* update the header */
	if(writen(outfd, "Y\n",2) < 0){
		log_error(LOG_ARGS, "Error writting extra header");
		return -1;
	}

	/* seek back to the end of the mail */
	if(lseek(outfd,0,SEEK_END) < 0) {
		log_error(LOG_ARGS,"Error seeking to the mail end");
		return -1;
	}
	return 0;
}

static int parse_body(int infd,int outfd, struct strlist* delmime, struct strlist* denymime,
			int* deny,char* boundary){
	int strip = 0;
	char* line;
	while((line = mygetline(infd))) {
		if(boundary && !strncmp(line,boundary,strlen(boundary))){
			strip = 0;
			/* check if the boundary is the beginning of a new part */
			if(strncmp(line + strlen(boundary),"--",2)){
				struct strlist hdrs;
				char* new_boundary = NULL;
				/* check if this part should be stripped */
				if(read_hdrs(infd, &hdrs,delmime,denymime,deny,&new_boundary) == MIME_STRIP)
					strip = 1;
				else {
					/* write boundary */
					if(writen(outfd, line, strlen(line)) < 0){
						log_error(LOG_ARGS, "Error writting boundary");
						return -1;
					}
					/* write hdr */
					if(write_hdrs(outfd, &hdrs, NULL) < 0){
						log_error(LOG_ARGS, "Error writting hdrs");
						return -1;
					}
					/* parse embedded part if a new boundary was found */
					if(new_boundary && parse_body(infd,outfd,delmime,denymime,deny,new_boundary) != 0) {
						log_error(LOG_ARGS, "Could not parse embedded part");
						return -1;
					}
				}
				free_strlist(&hdrs);
				if(new_boundary)
					myfree(new_boundary);
			}else{
				/* write end of part */
				if(writen(outfd, line, strlen(line)) < 0){
					log_error(LOG_ARGS, "Error writting hdrs");
					return -1;
				}
				/* and leave */
				myfree(line);
				break;
			}
		}else {
			if(!strip) { /* write the current line */
				if(writen(outfd, line, strlen(line)) < 0){
					log_error(LOG_ARGS, "Error when dumping line");
					return -1;
				}
			}
		}
		myfree(line);
	}
	return 0;
}





/* read a mail stripping unwanted parts */
static int dump_mail(int infd, int outfd,char* listdir) {
	struct strlist hdrs;
	struct strlist* delmime;
	struct strlist* denymime;
	char* boundary=NULL;
	int deny = 0;
	int result;
	off_t unwantedmime_hdr_pos = 0;

	/* get list control values */
	delmime = ctrlvalues(listdir, "mimestrip");
	denymime = ctrlvalues(listdir, "mimedeny");

	/* read mail header */
	result = read_hdrs(infd, &hdrs,delmime,denymime,&deny,&boundary);
	/* write mail header */
	if(write_hdrs(outfd,&hdrs,&unwantedmime_hdr_pos) < 0) {
		log_error(LOG_ARGS, "Could not write mail headers");
		return -1;
	}

	/* free mail header */
	free_strlist(&hdrs);

	if(result == MIME_OK && !deny) {
		/* try to parse the mail */
		if(parse_body(infd,outfd,delmime,denymime,&deny,boundary) != 0) {
			log_error(LOG_ARGS, "Could not parse mail");
			return -1;
		}
		myfree(boundary);
	}else
		deny = 1;

	/* dump rest of mail */
        if(dumpfd2fd(infd, outfd) != 0) {
		log_error(LOG_ARGS, "Could not recieve mail");
		return -1;
        }

	/* update header */
	if(deny) {
		if(update_unwantedmime_hdr(outfd,unwantedmime_hdr_pos) != 0) {
			log_error(LOG_ARGS, "Could not update header");
			return -1;
		}
	}

	/* free mime types */
	if(delmime) {
		free_strlist(delmime);
		myfree(delmime);
	}
	if(denymime) {
		free_strlist(denymime);
		myfree(denymime);
	}
	return 0;
}

static void print_help(const char *prg)
{
        printf("Usage: %s -L /path/to/listdir [-h] [-V] [-P] [-F]\n"
	       " -h: This help\n"
	       " -F: Don't fork in the background\n"
	       " -L: Full path to list directory\n"
	       " -P: Don't execute mlmmj-process\n"
	       " -V: Print version\n", prg);
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	char *infilename = NULL, *listdir = NULL;
	char *randomstr = random_str();
	char *mlmmjprocess, *bindir;
	int fd, opt, noprocess = 0, nofork = 0;
	struct stat st;
	uid_t uid;
	pid_t childpid;

	CHECKFULLPATH(argv[0]);

	log_set_name(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjprocess = concatstr(2, bindir, "/mlmmj-process");
	myfree(bindir);

	while ((opt = getopt(argc, argv, "hPVL:F")) != -1) {
		switch(opt) {
		case 'h':
			print_help(argv[0]);
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'P':
			noprocess = 1;
			break;
		case 'F':
			nofork = 1;
			break;
		case 'V':
			print_version(argv[0]);
			exit(0);
		}
	}

	if(listdir == NULL) {
		fprintf(stderr, "You have to specify -L\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Lets make sure no random user tries to send mail to the list */
	if(listdir) {
		if(stat(listdir, &st) == 0) {
			uid = getuid();
			if(uid && uid != st.st_uid) {
				log_error(LOG_ARGS,
					"Have to invoke either as root "
					"or as the user owning listdir "
					"Invoked with uid = [%d]", (int)uid);
				writen(STDERR_FILENO,
					"Have to invoke either as root "
					"or as the user owning listdir\n", 60);
				exit(EXIT_FAILURE);
			}
		} else {
			log_error(LOG_ARGS, "Could not stat %s", listdir);
			exit(EXIT_FAILURE);
		}
	}

	infilename = concatstr(3, listdir, "/incoming/", randomstr);
	myfree(randomstr);
	fd = open(infilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	while(fd < 0 && errno == EEXIST) {
		myfree(infilename);
		randomstr = random_str();
		infilename = concatstr(3, listdir, "/incoming/", randomstr);
		myfree(randomstr);
		fd = open(infilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	}

	if(fd < 0) {
		log_error(LOG_ARGS, "could not create mail file in "
				    "%s/incoming directory", listdir);
		myfree(infilename);
		exit(EXIT_FAILURE);
	}

	if(dump_mail(fileno(stdin), fd, listdir) != 0) {
		log_error(LOG_ARGS, "Could not recieve mail");
		exit(EXIT_FAILURE);
	}

#if 0
	log_oper(listdir, OPLOGFNAME, "mlmmj-recieve got %s", infilename);
#endif
	fsync(fd);
	close(fd);

	if(noprocess) {
		myfree(infilename);
		exit(EXIT_SUCCESS);
	}

	/*
	 * Now we fork so we can exit with success since it could potentially
	 * take a long time for mlmmj-send to finish delivering the mails and
	 * returning, making it susceptible to getting a SIGKILL from the
	 * mailserver invoking mlmmj-recieve.
	 */
	if (!nofork) {
		childpid = fork();
		if(childpid < 0)
			log_error(LOG_ARGS, "fork() failed! Proceeding anyway");

		if(childpid)
			exit(EXIT_SUCCESS); /* Parent says: "bye bye kids!"*/

		close(0);
		close(1);
		close(2);
	}

	execlp(mlmmjprocess, mlmmjprocess,
				"-L", listdir,
				"-m", infilename, NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjprocess);

	exit(EXIT_FAILURE);
}

