/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>

#include "mlmmj-maintd.h"
#include "mlmmj.h"
#include "strgen.h"
#include "chomp.h"
#include "log_error.h"
#include "mygetline.h"

static void print_help(const char *prg)
{
	printf("Usage: %s -L /path/to/listdir [-F]\n"
	       " -L: Full path to list directory\n"
	       " -F: Don't fork, performing one maintenance run only.\n"
	       "     This option should be used when one wants to\n"
	       "     avoid running another daemon, and use e.g."
	       "     cron to control it instead.\n", prg);
	exit(EXIT_SUCCESS);
}

int delolder(const char *dirname, time_t than)
{
	DIR *dir;
	struct dirent *dp;
	struct stat st;
	time_t t;

	if(chdir(dirname) < 0) {
		log_error(LOG_ARGS, "Could not chdir(%s)", dirname);
		return -1;
	}
	if((dir = opendir(dirname)) == NULL) {
		log_error(LOG_ARGS, "Could not opendir(%s)", dirname);
		return -1;
	}

	while((dp = readdir(dir)) != NULL) {
		if(stat(dp->d_name, &st) < 0) {
			log_error(LOG_ARGS, "Could not stat(%s)",dp->d_name);
			continue;
		}
		if(!S_ISREG(st.st_mode))
			continue;
		t = time(NULL);
		if(t - st.st_mtime > than)
			unlink(dp->d_name);
	}
	closedir(dir);

	return 0;
}


int clean_moderation(const char *listdir)
{
	char *moddirname = concatstr(2, listdir, "/moderation");
	int ret = delolder(moddirname, MODREQLIFE);	
		
	free(moddirname);

	return ret;
}

int clean_discarded(const char *listdir)
{
	char *discardeddirname = concatstr(2, listdir, "/queue/discarded");
	int ret = delolder(discardeddirname, DISCARDEDLIFE);

	free(discardeddirname);

	return ret;
}

int discardmail(const char *old, const char *new, time_t age)
{
	struct stat st;
	time_t t;

	stat(old, &st);
	t = time(NULL);

	if(t - st.st_mtime > age) {
		rename(old, new);
		return 1;
	}

	return 0;
}

int resend_queue(const char *listdir, const char *mlmmjsend)
{
	DIR *queuedir;
	struct dirent *dp;
	char *mailname, *fromname, *toname, *reptoname, *from, *to, *repto;
	char *discardedname = NULL, *ch;
	char *dirname = concatstr(2, listdir, "/queue/");
	pid_t pid;
	struct stat st;
	int fromfd, tofd, fd, discarded = 0;

	if(chdir(dirname) < 0) {
		log_error(LOG_ARGS, "Could not chdir(%s)", dirname);
		free(dirname);
		return 1;
	}
		
	if((queuedir = opendir(dirname)) == NULL) {
		log_error(LOG_ARGS, "Could not opendir(%s)", dirname);
		free(dirname);
		return 1;
	}
	free(dirname);

	while((dp = readdir(queuedir)) != NULL) {
		if(stat(dp->d_name, &st) < 0) {
			log_error(LOG_ARGS, "Could not stat(%s)",dp->d_name);
			continue;
		}

		if(!S_ISREG(st.st_mode))
			continue;

		if(strchr(dp->d_name, '.')) {
			mailname = strdup(dp->d_name);
			ch = strchr(mailname, '.');
			*ch = '\0';
			if(stat(mailname, &st) < 0)
				if(errno == ENOENT)
					unlink(dp->d_name);
			free(mailname);
			continue;
		}

		mailname = concatstr(3, listdir, "/queue/", dp->d_name);

		fromname = concatstr(2, mailname, ".mailfrom");
		if(stat(fromname, &st) < 0) {
			if(errno == ENOENT) {
				discardedname = concatstr(3,
						listdir, "/queue/discarded/",
						dp->d_name);
				discarded = discardmail(mailname,
							discardedname,
							3600);
			} else {
				log_error(LOG_ARGS, "Could not stat(%s)",
						dp->d_name);
			}
		}

		toname = concatstr(2, mailname, ".reciptto");
		if(!discarded && stat(toname, &st) < 0) {
			if(errno == ENOENT) {
				discardedname = concatstr(3,
						listdir, "/queue/discarded/",
						dp->d_name);
				discarded = discardmail(mailname,
							discardedname,
							3600);
			} else {
				log_error(LOG_ARGS, "Could not stat(%s)",
						dp->d_name);
			}
		}
		
		reptoname = concatstr(2, mailname, ".reply-to");

		fromfd = open(fromname, O_RDONLY);
		tofd = open(toname, O_RDONLY);

		if(fromfd < 0 || tofd < 0) {
			if(discarded) {
				unlink(fromname);
				unlink(toname);
				unlink(reptoname);
			}
			free(mailname);
			free(fromname);
			free(toname);
			free(reptoname);
			if(fromfd >= 0)
				close(fromfd);
			continue;
		}

		from = mygetline(fromfd);
		chomp(from);
		close(fromfd);
		unlink(fromname);
		free(fromname);
		to = mygetline(tofd);
		chomp(to);
		close(tofd);
		unlink(toname);
		free(toname);
		fd = open(reptoname, O_RDONLY);
		if(fd < 0) {
			free(reptoname);
			repto = NULL;
		} else {
			repto = mygetline(fd);
			chomp(repto);
			close(fd);
			unlink(reptoname);
			free(reptoname);
		}

		pid = fork();

		if(pid == 0) {
			if(repto)
				execlp(mlmmjsend, mlmmjsend,
						"-l", "1",
						"-m", mailname,
						"-F", from,
						"-T", to,
						"-R", repto,
						"-a", 0);
			else
				execlp(mlmmjsend, mlmmjsend,
						"-l", "1",
						"-m", mailname,
						"-F", from,
						"-T", to,
						"-a", 0);
		}
	}

	return 0;
}

int resend_requeue(const char *listdir, const char *mlmmjsend)
{
	DIR *queuedir;
	struct dirent *dp;
	char *dirname = concatstr(2, listdir, "/requeue/");
	char *archivefilename, *subfilename, *subnewname;
	struct stat st;
	pid_t pid;
	time_t t;

	if(chdir(dirname) < 0) {
		log_error(LOG_ARGS, "Could not chdir(%s)", dirname);
		free(dirname);
		return 1;
	}
		
	if((queuedir = opendir(dirname)) == NULL) {
		log_error(LOG_ARGS, "Could not opendir(%s)", dirname);
		free(dirname);
		return 1;
	}

	while((dp = readdir(queuedir)) != NULL) {
		if((strcmp(dp->d_name, "..") == 0) ||
			(strcmp(dp->d_name, ".") == 0))
				continue;

		if(stat(dp->d_name, &st) < 0) {
			log_error(LOG_ARGS, "Could not stat(%s)",dp->d_name);
			continue;
		}

		if(!S_ISDIR(st.st_mode))
			continue;

		/* Remove old empty directories */
		t = time(NULL);
		if(t - st.st_mtime > (time_t)3600)
			if(rmdir(dp->d_name) == 0)
				continue;

		archivefilename = concatstr(3, listdir, "/archive/",
						dp->d_name);
		if(stat(archivefilename, &st) < 0) {
			/* Might be it's just not moved to the archive
			 * yet because it's still getting sent, so just
			 * continue
			 */
			free(archivefilename);
			continue;
		}
		subfilename = concatstr(3, dirname, dp->d_name, "/subscribers");
		if(stat(subfilename, &st) < 0) {
			log_error(LOG_ARGS, "Could not stat(%s)", subfilename);
			free(archivefilename);
			free(subfilename);
			continue;
		}

		subnewname = concatstr(2, subfilename, ".resending");

		if(rename(subfilename, subnewname) < 0) {
			log_error(LOG_ARGS, "Could not rename(%s, %s)",
						subfilename, subnewname);
			free(archivefilename);
			free(subfilename);
			free(subnewname);
			continue;
		}
		free(subfilename);
		
		pid = fork();

		if(pid == 0)
			execlp(mlmmjsend, mlmmjsend,
					"-l", "3",
					"-L", listdir,
					"-m", archivefilename,
					"-s", subnewname,
					"-a",
					"-D", 0);
	}

	return 0;
}

int clean_nolongerbouncing(const char *listdir)
{
	DIR *bouncedir;
	char *dirname = concatstr(2, listdir, "/bounce/");
	char *filename, *probetimestr, *s;
	int probefd;
	time_t probetime, t;
	struct dirent *dp;
	struct stat st;
	
	if(chdir(dirname) < 0) {
		log_error(LOG_ARGS, "Could not chdir(%s)", dirname);
		free(dirname);
		return 1;
	}
		
	if((bouncedir = opendir(dirname)) == NULL) {
		log_error(LOG_ARGS, "Could not opendir(%s)", dirname);
		free(dirname);
		return 1;
	}

	while((dp = readdir(bouncedir)) != NULL) {
		if((strcmp(dp->d_name, "..") == 0) ||
		   (strcmp(dp->d_name, ".") == 0))
				continue;

		if(stat(dp->d_name, &st) < 0) {
			log_error(LOG_ARGS, "Could not stat(%s)", dp->d_name);
			continue;
		}

		filename = strdup(dp->d_name);

		if((s = strstr(filename, "-probe"))) {
			probefd = open(filename, O_RDONLY);
			if(probefd < 0)
				continue;
			probetimestr = mygetline(probefd);
			if(probetimestr == NULL)
				continue;
			close(probefd);
			chomp(probetimestr);
			probetime = (time_t)strtol(probetimestr, NULL, 10);
			t = time(NULL);
			if(t - probetime > WAITPROBE) {
				unlink(filename);
				*s = '\0';
				unlink(filename);
			}
		}
		free(filename);
	}

	return 0;
}

int probe_bouncers(const char *listdir)
{

	/* XXX: This is too expensive to do here with an
	 * execlp for each one ... */

	return 0;
}

int unsub_bouncers(const char *listdir)
{
#if 0
	DIR *bouncedir;
	struct dirent *dp;
#endif

	/* TODO: Unsubscribe all that still bounces after BOUNCELIFE time
	 * (control/bouncelife later on )
	 */

	return 0;
}

int main(int argc, char **argv)
{
	int opt, daemonize = 1;
	char *bindir, *listdir = NULL, *mlmmjsend;

	log_set_name(argv[0]);

	while ((opt = getopt(argc, argv, "hFVL:")) != -1) {
		switch(opt) {
		case 'F':
			daemonize = 0;
			break;
		case 'L':
			listdir = optarg;
			break;
		case 'h':
			print_help(argv[0]);
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
	
	if(chdir(listdir) < 0) {
		log_error(LOG_ARGS, "Could not chdir(%s), exiting. "
				    "No maintenance performed.", listdir);
		exit(EXIT_FAILURE);
	}

	bindir = mydirname(argv[0]);
	mlmmjsend = concatstr(2, bindir, "/mlmmj-send");
	free(bindir);

	if(daemonize && daemon(1,0) < 0) {
		log_error(LOG_ARGS, "Could not daemonize. Only one "
				"maintenance run will be done.");
		daemonize = 0;
	}

	for(;;) {
		clean_moderation(listdir);
		clean_discarded(listdir);
		resend_queue(listdir, mlmmjsend);
		resend_requeue(listdir, mlmmjsend);
		clean_nolongerbouncing(listdir);
		probe_bouncers(listdir);
		unsub_bouncers(listdir);
		
		if(daemonize == 0)
			break;
		else
			sleep(MAINTD_SLEEP);
	}
		
	exit(EXIT_SUCCESS);
}
