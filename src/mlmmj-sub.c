/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
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
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/wait.h>
#include <ctype.h>

#include "mlmmj.h"
#include "mlmmj-sub.h"
#include "mylocking.h"
#include "wrappers.h"
#include "getlistaddr.h"
#include "getlistdelim.h"
#include "strgen.h"
#include "subscriberfuncs.h"
#include "log_error.h"
#include "mygetline.h"
#include "statctrl.h"
#include "prepstdreply.h"
#include "memory.h"
#include "ctrlvalues.h"
#include "chomp.h"

char *subtype_strs[] = {
	"normal",
	"digest",
	"nomail",
	"file",
	"all"
};

char * subreason_strs[] = {
	"request",
	"confirm",
	"permit",
	"admin",
	"bouncing"
};

static void moderate_sub(const char *listdir, const char *listaddr,
		const char *listdelim, const char *subaddr,
		const char *mlmmjsend, enum subtype typesub, enum subreason reasonsub)
{
	int i, fd, status, nosubmodmails = 0;
	text *txt;
	memory_lines_state *mls;
	char *a = NULL, *queuefilename, *from, *listname, *listfqdn, *str;
	char *modfilename, *mods, *to, *replyto, *moderators = NULL;
	char *cookie, *obstruct;
	struct strlist *submods;
	pid_t childpid, pid;

	/* generate the file in moderation/ */
	switch(typesub) {
		default:
		case SUB_NORMAL:
			str = concatstr(4, subaddr, "\n", "SUB_NORMAL", "\n");
			break;
		case SUB_DIGEST:
			str = concatstr(4, subaddr, "\n", "SUB_DIGEST", "\n");
			break;
		case SUB_NOMAIL:
			str = concatstr(4, subaddr, "\n", "SUB_NOMAIL", "\n");
			break;
	}
	
	for (;;) {
		cookie = random_str();
		modfilename = concatstr(3, listdir, "/moderation/subscribe",
				cookie);
		fd = open(modfilename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
		if (fd < 0) {
			if (errno == EEXIST) {
				myfree(cookie);
				myfree(modfilename);
				continue;
			}
			log_error(LOG_ARGS, "could not create %s"
					"ignoring request: %s", str);
			exit(EXIT_FAILURE);
		}
		break;
	}

	if(writen(fd, str, strlen(str)) < 0) {
		log_error(LOG_ARGS, "could not write to %s"
				"ignoring request: %s", str);
		exit(EXIT_FAILURE);
	}
	
	close(fd);
	
	myfree(str);

	submods = ctrlvalues(listdir, "submod");
	mods = concatstr(2, listdir, "/control/submod");
	/* check to see if there's adresses in the submod control file */
	for(i = 0; i < submods->count; i++)
		a = strchr(submods->strs[i], '@');

	/* no addresses in submod control file, use owner */
	if(a == NULL) {
		/* free the submods struct from above */
		for(i = 0; i < submods->count; i++)
			myfree(submods->strs[i]);
		myfree(submods->strs);
		myfree(submods);
		submods = ctrlvalues(listdir, "owner");
		myfree(mods);
		mods = concatstr(2, listdir, "/control/owner");
	}

	/* send mail to moderators about request pending */
	listdelim = getlistdelim(listdir);
	listfqdn = genlistfqdn(listaddr);
	listname = genlistname(listaddr);

	from = concatstr(4, listname, listdelim, "owner@", listfqdn);
	to = concatstr(3, listname, "-moderators@", listfqdn);
	replyto = concatstr(6, listname, listdelim, "permit-", cookie,
			"@", listfqdn);
	obstruct = concatstr(6, listname, listdelim, "obstruct-", cookie,
			"@", listfqdn);
	myfree(cookie);
	for(i = 0; i < submods->count; i++) {
		printf("%s", submods->strs[i]);
		str = moderators;
		moderators = concatstr(3, moderators, submods->strs[i], "\n");
		myfree(str);
	}
	mls = init_memory_lines(moderators);
	myfree(moderators);

	txt = open_text(listdir,
			"gatekeep", "sub",
			subreason_strs[reasonsub], subtype_strs[typesub],
			"submod-moderator");
	MY_ASSERT(txt);
	register_unformatted(txt, "subaddr", subaddr);
	register_unformatted(txt, "moderateaddr", replyto); /* DEPRECATED */
	register_unformatted(txt, "permitaddr", replyto);
	register_unformatted(txt, "obstructaddr", obstruct);
	register_unformatted(txt, "moderators", "%gatekeepers%"); /* DEPRECATED */
	register_formatted(txt, "gatekeepers",
			rewind_memory_lines, get_memory_line, mls);
	queuefilename = prepstdreply(txt, listdir, "$listowner$", to, replyto);
	MY_ASSERT(queuefilename);
	close_text(txt);
	
	/* we might need to exec more than one mlmmj-send */
	
	nosubmodmails = statctrl(listdir,"nosubmodmails");
	
	if (nosubmodmails)
		childpid = -1;
	else {
		childpid = fork();
		if(childpid < 0)
			log_error(LOG_ARGS, "Could not fork; requester not notified");
	}

	if(childpid != 0) {
		if(childpid > 0) {
			do /* Parent waits for the child */
				pid = waitpid(childpid, &status, 0);
			while(pid == -1 && errno == EINTR);
		}
		finish_memory_lines(mls);
		execl(mlmmjsend, mlmmjsend,
				"-a",
				"-l", "4",
				"-L", listdir,
				"-s", mods,
				"-F", from,
				"-R", replyto,
				"-m", queuefilename, (char *)NULL);
		log_error(LOG_ARGS, "execl() of '%s' failed", mlmmjsend);
		exit(EXIT_FAILURE);
	}

	myfree(to);
	myfree(replyto);
	
	/* send mail to requester that the list is submod'ed */

	from = concatstr(4, listname, listdelim, "bounces-help@", listfqdn);

	txt = open_text(listdir,
			"wait", "sub",
			subreason_strs[reasonsub], subtype_strs[typesub],
			"submod-requester");
	MY_ASSERT(txt);
	register_unformatted(txt, "subaddr", subaddr);
	register_unformatted(txt, "moderators", "%gatekeepers"); /* DEPRECATED */
	register_formatted(txt, "gatekeepers",
			rewind_memory_lines, get_memory_line, mls);
	queuefilename = prepstdreply(txt, listdir,
			"$listowner$", subaddr, NULL);
	MY_ASSERT(queuefilename);
	close_text(txt);

	finish_memory_lines(mls);
	myfree(listname);
	myfree(listfqdn);
	execl(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-L", listdir,
				"-T", subaddr,
				"-F", from,
				"-m", queuefilename, (char *)NULL);
	log_error(LOG_ARGS, "execl() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void getaddrandtype(const char *listdir, const char *modstr,
		char **addrptr, enum subtype *subtypeptr)
{
	int fd;
	char *readaddr, *readtype, *modfilename;

	if (strncmp(modstr, "subscribe", 9) == 0)
			modstr += 9;

	modfilename = concatstr(3, listdir, "/moderation/subscribe", modstr);

	fd = open(modfilename, O_RDONLY);
	if(fd < 0) {
		log_error(LOG_ARGS, "Could not open %s", modfilename);
		exit(EXIT_FAILURE);
	}

	readaddr = mygetline(fd);
	readtype = mygetline(fd);

	close(fd);

	if(readaddr == NULL || readtype == NULL) {
		log_error(LOG_ARGS, "Could not parse %s", modfilename);
		exit(EXIT_FAILURE);
	}
	
	chomp(readaddr);
	*addrptr = readaddr;

	if(strncmp(readtype, "SUB_NORMAL", 10) == 0) {
		*subtypeptr = SUB_NORMAL;
		goto freedone;
	}

	if(strncmp(readtype, "SUB_DIGEST", 10) == 0) {
		*subtypeptr = SUB_DIGEST;
		goto freedone;
	}

	if(strncmp(readtype, "SUB_NOMAIL", 10) == 0) {
		*subtypeptr = SUB_NOMAIL;
		goto freedone;
	}

	log_error(LOG_ARGS, "Type %s not valid in %s", readtype,
			modfilename);

freedone:
	myfree(readtype);
	unlink(modfilename);
	myfree(modfilename);
}

void confirm_sub(const char *listdir, const char *listaddr,
		const char *listdelim, const char *subaddr,
		const char *mlmmjsend, enum subtype typesub, enum subreason reasonsub)
{
	text *txt;
	char *queuefilename, *fromaddr, *listname, *listfqdn, *listtext;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

	fromaddr = concatstr(4, listname, listdelim, "bounces-help@", listfqdn);

	myfree(listname);
	myfree(listfqdn);

	switch(typesub) {
		default:
		case SUB_NORMAL:
			listtext = mystrdup("sub-ok");
			break;
		case SUB_DIGEST:
			listtext = mystrdup("sub-ok-digest");
			break;
		case SUB_NOMAIL:
			listtext = mystrdup("sub-ok-nomail");
			break;
	}

	txt = open_text(listdir, "finish", "sub",
			subreason_strs[reasonsub], subtype_strs[typesub],
			listtext);
	myfree(listtext);
	MY_ASSERT(txt);
	register_unformatted(txt, "subaddr", subaddr);
	queuefilename = prepstdreply(txt, listdir,
			"$helpaddr$", subaddr, NULL);
	MY_ASSERT(queuefilename);
	close_text(txt);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-L", listdir,
				"-T", subaddr,
				"-F", fromaddr,
				"-m", queuefilename, (char *)NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void notify_sub(const char *listdir, const char *listaddr,
		const char *listdelim, const char *subaddr,
		const char *mlmmjsend, enum subtype typesub, enum subreason reasonsub)
{
	char *listfqdn, *listname, *fromaddr, *tostr;
	text *txt;
	char *queuefilename = NULL, *listtext = NULL;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

	fromaddr = concatstr(4, listname, listdelim, "bounces-help@", listfqdn);
	tostr = concatstr(4, listname, listdelim, "owner@", listfqdn);
	
	myfree(listname);
	myfree(listfqdn);

	switch(typesub) {
		default:
		case SUB_NORMAL:
			listtext = mystrdup("notifysub");
			break;
		case SUB_DIGEST:
			listtext = mystrdup("notifysub-digest");
			break;
		case SUB_NOMAIL:
			listtext = mystrdup("notifysub-nomail");
			break;
	}

	txt = open_text(listdir, "notify", "sub",
			subreason_strs[reasonsub], subtype_strs[typesub],
			listtext);
	myfree(listtext);
	MY_ASSERT(txt);
	register_unformatted(txt, "subaddr", subaddr);
	register_unformatted(txt, "newsub", subaddr); /* DEPRECATED */
	queuefilename = prepstdreply(txt, listdir,
			"$listowner$", "$listowner$", NULL);
	MY_ASSERT(queuefilename);
	close_text(txt);

	execlp(mlmmjsend, mlmmjsend,
			"-l", "1",
			"-L", listdir,
			"-T", tostr,
			"-F", fromaddr,
			"-m", queuefilename, (char *)NULL);

	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

void generate_subconfirm(const char *listdir, const char *listaddr,
			 const char *listdelim, const char *subaddr,
			 const char *mlmmjsend, enum subtype typesub, enum subreason reasonsub)
{
	int subconffd;
	char *confirmaddr, *listname, *listfqdn, *confirmfilename = NULL;
	text *txt;
	char *listtext, *queuefilename = NULL, *fromaddr;
	char *randomstr = NULL, *tmpstr;

	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

        do {
                myfree(confirmfilename);
                myfree(randomstr);
		randomstr = random_plus_addr(subaddr);
                confirmfilename = concatstr(3, listdir, "/subconf/",
					    randomstr);

                subconffd = open(confirmfilename, O_RDWR|O_CREAT|O_EXCL,
						  S_IRUSR|S_IWUSR);

        } while ((subconffd < 0) && (errno == EEXIST));

	if(subconffd < 0) {
		log_error(LOG_ARGS, "Could not open '%s'", confirmfilename);
		myfree(confirmfilename);
                myfree(randomstr);
		exit(EXIT_FAILURE);
	}

	myfree(confirmfilename);

	if(writen(subconffd, subaddr, strlen(subaddr)) < 0) {
		log_error(LOG_ARGS, "Could not write to subconffd");
		myfree(confirmfilename);
                myfree(randomstr);
		exit(EXIT_FAILURE);
	}

	close(subconffd);

	fromaddr = concatstr(6, listname, listdelim, "bounces-confsub-",
				randomstr, "@", listfqdn);
	
	switch(typesub) {
		default:
		case SUB_NORMAL:
			listtext = mystrdup("sub-confirm");
			tmpstr = mystrdup("confsub-");
			break;
		case SUB_DIGEST:
			listtext = mystrdup("sub-confirm-digest");
			tmpstr = mystrdup("confsub-digest-");
			break;
		case SUB_NOMAIL:
			listtext = mystrdup("sub-confirm-nomail");
			tmpstr = mystrdup("confsub-nomail-");
			break;
	}

	confirmaddr = concatstr(6, listname, listdelim, tmpstr, randomstr, "@",
				listfqdn);

	myfree(randomstr);
	myfree(tmpstr);

	txt = open_text(listdir, "confirm", "sub",
			subreason_strs[reasonsub], subtype_strs[typesub],
			listtext);
	myfree(listtext);
	MY_ASSERT(txt);
	register_unformatted(txt, "subaddr", subaddr);
	register_unformatted(txt, "confaddr", confirmaddr); /* DEPRECATED */
	register_unformatted(txt, "confirmaddr", confirmaddr);
	queuefilename = prepstdreply(txt, listdir,
			"$helpaddr$", subaddr, confirmaddr);
	MY_ASSERT(queuefilename);
	close_text(txt);

	myfree(listname);
	myfree(listfqdn);

	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-L", listdir,
				"-T", subaddr,
				"-F", fromaddr,
				"-m", queuefilename, (char *)NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);
	exit(EXIT_FAILURE);
}

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/list {-a john@doe.org | -m str}\n"
	       "       [-c | -C] [-f] [-h] [-L] [-d | -n] [-r | -R] [-s] [-U] [-V]\n"
	       " -a: Email address to subscribe \n"
	       " -c: Send welcome mail\n"
	       " -C: Request mail confirmation\n"
	       " -d: Subscribe to digest of list\n"
	       " -f: Force subscription (do not moderate)\n"
	       " -h: This help\n"
	       " -L: Full path to list directory\n"
	       " -m: moderation string\n"
	       " -n: Subscribe to no mail version of list\n", prg);
	printf(" -r: Behave as if request arrived via email (internal use)\n"
	       " -R: Behave as if confirmation arrived via email (internal use)\n"
	       " -s: Don't send a mail to subscriber if already subscribed\n"
	       " -U: Don't switch to the user id of the listdir owner\n"
	       " -V: Print version\n"
	       "When no options are specified, subscription may be "
	       "moderated;\nto ensure a silent subscription, use -f\n");
	exit(EXIT_SUCCESS);
}

void generate_subscribed(const char *listdir, const char *subaddr,
		const char *mlmmjsend)
{
	text *txt;
	char *queuefilename, *fromaddr, *listname, *listfqdn, *listaddr;
	char *listdelim = getlistdelim(listdir);

	listaddr = getlistaddr(listdir);
	listname = genlistname(listaddr);
	listfqdn = genlistfqdn(listaddr);

	fromaddr = concatstr(4, listname, listdelim, "bounces-help@", listfqdn);
	myfree(listdelim);

	txt = open_text(listdir,
			"deny", "sub", "subbed", NULL, "sub-subscribed");
	MY_ASSERT(txt);
	register_unformatted(txt, "subaddr", subaddr);
	queuefilename = prepstdreply(txt, listdir,
			"$helpaddr$", subaddr, NULL);
	MY_ASSERT(queuefilename);
	close_text(txt);

	myfree(listaddr);
	myfree(listname);
	myfree(listfqdn);
	
	execlp(mlmmjsend, mlmmjsend,
				"-l", "1",
				"-L", listdir,
				"-T", subaddr,
				"-F", fromaddr,
				"-m", queuefilename, (char *)NULL);
	log_error(LOG_ARGS, "execlp() of '%s' failed", mlmmjsend);

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	char *listaddr, *listdelim, *listdir = NULL, *address = NULL;
	char *subfilename = NULL, *mlmmjsend, *bindir, chstr[2], *subdir;
	char *subddirname = NULL, *sublockname, *lowcaseaddr;
	char *modstr = NULL;
	int subconfirm = 0, confirmsub = 0, opt, subfilefd, lock, notifysub;
	int changeuid = 1, status, digest = 0, nomail = 0, i = 0, submod = 0;
	int groupwritable = 0, sublock, sublockfd, nogensubscribed = 0, subbed;
	int force = 0;
	size_t len;
	struct stat st;
	pid_t pid, childpid;
	uid_t uid;
	enum subtype typesub = SUB_NORMAL;
	enum subreason reasonsub = SUB_ADMIN;

	CHECKFULLPATH(argv[0]);

	log_set_name(argv[0]);

	bindir = mydirname(argv[0]);
	mlmmjsend = concatstr(2, bindir, "/mlmmj-send");
	myfree(bindir);

	while ((opt = getopt(argc, argv, "hcCdfm:nsVUL:a:rR")) != -1) {
		switch(opt) {
		case 'a':
			address = optarg;
			break;
		case 'c':
			confirmsub = 1;
			break;
		case 'C':
			subconfirm = 1;
			break;
		case 'd':
			digest = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'h':
			print_help(argv[0]);
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'm':
			modstr = optarg;
			break;
		case 'n':
			nomail = 1;
			break;
		case 'r':
			reasonsub = SUB_REQUEST;
			break;
		case 'R':
			reasonsub = SUB_CONFIRM;
			break;
		case 's':
			nogensubscribed = 1;
			break;
		case 'U':
			changeuid = 0;
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
	
	if(address == NULL && modstr == NULL) {
		fprintf(stderr, "You have to specify -a or -m\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(modstr) {
		getaddrandtype(listdir, modstr, &address, &typesub);
		reasonsub = SUB_PERMIT;
	}

	if(strchr(address, '@') == NULL) {
		log_error(LOG_ARGS, "No '@' sign in '%s', not subscribing",
				address);
		exit(EXIT_SUCCESS);
	}

	if(digest && nomail) {
		fprintf(stderr, "Specify either -d or -n, not both\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(digest)
		typesub = SUB_DIGEST;
	if(nomail)
		typesub = SUB_NOMAIL;

	if(confirmsub && subconfirm) {
		fprintf(stderr, "Cannot specify both -C and -c\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if(reasonsub == SUB_CONFIRM && subconfirm) {
		fprintf(stderr, "Cannot specify both -C and -R\n");
		fprintf(stderr, "%s -h for help\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Make the address lowercase */
	lowcaseaddr = mystrdup(address);
	i = 0;
	while(lowcaseaddr[i]) {
		lowcaseaddr[i] = tolower(lowcaseaddr[i]);
		i++;
	}
	address = lowcaseaddr;

	/* get the list address */
	listaddr = getlistaddr(listdir);
	if(strncasecmp(listaddr, address, strlen(listaddr)) == 0) {
		printf("Cannot subscribe the list address to the list\n");
		exit(EXIT_SUCCESS);  /* XXX is this success? */
	}

	switch(typesub) {
		default:
		case SUB_NORMAL:
			subdir = "/subscribers.d/";
			break;
		case SUB_DIGEST:
			subdir = "/digesters.d/";
			break;
		case SUB_NOMAIL:
			subdir = "/nomailsubs.d/";
			break;
	}

	subddirname = concatstr(2, listdir, subdir);
	if (stat(subddirname, &st) == 0) {
		if(st.st_mode & S_IWGRP) {
			groupwritable = S_IRGRP|S_IWGRP;
			umask(S_IWOTH);
			setgid(st.st_gid);
		}
	}

	if(changeuid) {
		uid = getuid();
		if(!uid && stat(listdir, &st) == 0) {
			printf("Changing to uid %d, owner of %s.\n",
					(int)st.st_uid, listdir);
			if(setuid(st.st_uid) < 0) {
				perror("setuid");
				fprintf(stderr, "Continuing as uid %d\n",
						(int)uid);
			}
		}
	}

	chstr[0] = address[0];
	chstr[1] = '\0';
	
	subfilename = concatstr(3, listdir, subdir, chstr);

	sublockname = concatstr(5, listdir, subdir, ".", chstr, ".lock");
	sublockfd = open(sublockname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if(sublockfd < 0) {
		log_error(LOG_ARGS, "Error opening lock file %s",
				sublockname);
		myfree(sublockname);
		exit(EXIT_FAILURE);
	}

	sublock = myexcllock(sublockfd);
	if(sublock < 0) {
		log_error(LOG_ARGS, "Error locking '%s' file",
				sublockname);
		myfree(sublockname);
		close(sublockfd);
		exit(EXIT_FAILURE);
	}

	subfilefd = open(subfilename, O_RDWR|O_CREAT,
				S_IRUSR|S_IWUSR|groupwritable);
	if(subfilefd == -1) {
		log_error(LOG_ARGS, "Could not open '%s'", subfilename);
		myfree(sublockname);
		exit(EXIT_FAILURE);
	}

	lock = myexcllock(subfilefd);
	if(lock) {
		log_error(LOG_ARGS, "Error locking subscriber file");
		close(subfilefd);
		close(sublockfd);
		myfree(sublockname);
		exit(EXIT_FAILURE);
	}
	subbed = is_subbed_in(subddirname, address);
	listdelim = getlistdelim(listdir);
	
	if(subbed) {
		if(subconfirm) {
			close(subfilefd);
			close(sublockfd);
			unlink(sublockname);
			myfree(sublockname);
			generate_subconfirm(listdir, listaddr, listdelim,
					    address, mlmmjsend, typesub, reasonsub);
		} else {
			if(modstr == NULL)
				submod = !force && statctrl(listdir, "submod");
			if(submod) {
				close(subfilefd);
				close(sublockfd);
				unlink(sublockname);
				myfree(sublockname);
				moderate_sub(listdir, listaddr, listdelim,
					address, mlmmjsend, typesub, reasonsub);
			}
			lseek(subfilefd, 0L, SEEK_END);
			len = strlen(address);
			address[len] = '\n';
			writen(subfilefd, address, len + 1);
			address[len] = 0;
			close(subfilefd);
			close(sublockfd);
			unlink(sublockname);
		}
	} else {
		close(subfilefd);
		myfree(subfilename);
		close(sublockfd);
		unlink(sublockname);
		myfree(sublockname);

		if(!nogensubscribed)
			generate_subscribed(listdir, address, mlmmjsend);
		
		return EXIT_SUCCESS;
	}

	close(sublockfd);
	unlink(sublockname);
	myfree(sublockname);

	if(confirmsub) {
		childpid = fork();

		if(childpid < 0) {
			log_error(LOG_ARGS, "Could not fork; owner not notified");
			confirm_sub(listdir, listaddr, listdelim, address,
					mlmmjsend, typesub, reasonsub);
		}
		
		if(childpid > 0) {
			do /* Parent waits for the child */
				pid = waitpid(childpid, &status, 0);
			while(pid == -1 && errno == EINTR);
		}

		/* child confirms subscription */
		if(childpid == 0)
			confirm_sub(listdir, listaddr, listdelim, address,
					mlmmjsend, typesub, reasonsub);
	}

	notifysub = statctrl(listdir, "notifysub");

	/* Notify list owner about subscription */
	if (notifysub)
		notify_sub(listdir, listaddr, listdelim, address, mlmmjsend,
				typesub, reasonsub);

	myfree(address);
	myfree(listaddr);
	myfree(listdelim);

	return EXIT_SUCCESS;
}
