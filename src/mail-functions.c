/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include "mail-functions.h"
#include "wrappers.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "log_error.h"

/* "HELO \r\n " has length 7 */
#define EXTRA_HELO_LEN 8
int write_helo(int sockfd, const char *hostname)
{
	size_t len = (size_t)(strlen(hostname) + EXTRA_HELO_LEN);
	char *helo;
	size_t bytes_written;
	
	if((helo = malloc(len)) == 0)
		return errno;
	snprintf(helo, len, "HELO %s\r\n", hostname);
	len = strlen(helo);
#if 0
	fprintf(stderr, "\nwrite_helo, helo = [%s]\n", helo);
#endif
	bytes_written = writen(sockfd, helo, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write HELO");
		free(helo);
		return errno;
	}
	free(helo);
	return 0;
}
/* "MAIL FROM: <>\r\n" has length 15 */
#define EXTRA_FROM_LEN 16
int write_mail_from(int sockfd, const char *from_addr)
{
	size_t len = (size_t)(strlen(from_addr) + EXTRA_FROM_LEN);
	char *mail_from;
	size_t bytes_written;

	if((mail_from = malloc(len)) == NULL)
		return errno;
	snprintf(mail_from, len, "MAIL FROM: <%s>\r\n", from_addr);
	len = strlen(mail_from);

#ifdef MLMMJ_DEBUG
	fprintf(stderr, "\nwrite_mail_from, mail_from = [%s]\n", mail_from);
#endif
	bytes_written = writen(sockfd, mail_from, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write FROM");
		free(mail_from);
		return errno;
	}
	free(mail_from);
	return 0;
}

/* "RCPT TO: <>\r\n" has length 13 */
#define EXTRA_RCPT_LEN 14

int write_rcpt_to(int sockfd, const char *rcpt_addr)
{
	size_t len = (size_t)(strlen(rcpt_addr) + EXTRA_RCPT_LEN);
	char *rcpt_to;
	size_t bytes_written;
	
	if((rcpt_to = malloc(len)) == 0)
		return errno;

	snprintf(rcpt_to, len, "RCPT TO: <%s>\r\n", rcpt_addr);
	len = strlen(rcpt_to);

#ifdef MLMMJ_DEBUG
	fprintf(stderr, "\nwrite_rcpt_to, rcpt_to = [%s]\n", rcpt_to);
#endif
	bytes_written = writen(sockfd, rcpt_to, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write TO");
		free(rcpt_to);
		return errno;
	}
	free(rcpt_to);
	return 0;
}


int write_mailbody_from_file(int sockfd, FILE *infile)
{
	char buf[WRITE_BUFSIZE+3];
	size_t len, bytes_written;
	char *bufp;
	int full_line = 1;  /* true, if last write included a newline */
	
	/* Zero the buffer */
	memset(buf, 0, sizeof(buf));
	
	/* Read from beginning of the file */

	if(fseek(infile, 0L, SEEK_SET) == -1) {
		log_error(LOG_ARGS, "fseek() failed");
		return errno;
	}
	
	/* keep writing chunks of line (max WRITE_BUFSIZE) */
	for(;;) {
		bufp = buf+1;
		errno = 0;  /* We must reset errno, otherwise we can't
			     * determine if we hit EOF or an error
			     * occurred */
		if(!fgets(bufp, WRITE_BUFSIZE, infile)) {
			if (errno == EINTR) {
				errno = 0;
				continue;
			} else {
				return errno;
			}
		}

		/* fix "dot lines" */
		if(full_line && (bufp[0] == '.')) {
			*(--bufp) = '.';
			len++;
		}

		/* fix newlines */
		len = strlen(bufp);
		if((len > 0) && (bufp[len-1] == '\n')) {
			bufp[len-1] = '\r';
			bufp[len] = '\n';
			bufp[len+1] = 0;
			len++;
			full_line = 1;
		} else {
			full_line = 0;
		}

#ifdef MLMMJ_DEBUG
		fprintf(stderr, "write_mailbody_from_file = [%s]\n", bufp);
#endif
		bytes_written = writen(sockfd, bufp, len);
		if(bytes_written < 0 )
			return errno;
	}

	return 0;
}

/* "\r\n" has length 2 */
#define EXTRA_CUSTOM_LEN 3

int write_dot(int sockfd)
{
	size_t bytes_written;

	bytes_written = writen(sockfd, "\r\n.\r\n", 5);	
	if(bytes_written < 0)
		return errno;

	return 0;
}

int write_custom_line(int sockfd, const char *line)
{
	size_t len = strlen(line) + EXTRA_CUSTOM_LEN;
	size_t bytes_written;
	char *customline;
	
	if((customline = malloc(len)) == 0)
		return errno;
	
	snprintf(customline, len, "%s\r\n", line);
	len = strlen(customline);
#if 0
	fprintf(stderr, "write_custom_line = [%s]\n", customline);
	fprintf(stderr, "strlen(customline) = [%d]\n", strlen(customline));
#endif
	bytes_written = writen(sockfd, customline, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write customline");
		free(customline);
		return errno;
	}
	free(customline);
	return 0;
}

/* "Reply-To: \r\n" has length 12 */
#define EXTRA_REPLYTO_LEN 13

int write_replyto(int sockfd, const char *replyaddr)
{
	size_t len = (size_t)(strlen(replyaddr) + EXTRA_REPLYTO_LEN);
	char *replyto;
	size_t bytes_written;
	
	if((replyto = malloc(len)) == 0)
		return errno;

	snprintf(replyto, len, "Reply-To: %s\r\n", replyaddr);
	len = strlen(replyto);

#ifdef MLMMJ_DEBUG
	fprintf(stderr, "\nwrite_replyto, replyto = [%s]\n", replyto);
#endif
	bytes_written = writen(sockfd, replyto, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write Reply-To header");
		free(replyto);
		return errno;
	}
	free(replyto);
	return 0;
}

int write_data(int sockfd)
{
	if(write_custom_line(sockfd, "DATA")) {
		log_error(LOG_ARGS, "Could not write DATA");
		return errno;
	}

	return 0;
}

int write_quit(int sockfd)
{
	if(write_custom_line(sockfd, "QUIT")) {
		log_error(LOG_ARGS, "Could not write QUIT");
		return errno;
	}

	return 0;
}

int write_rset(int sockfd)
{
	if(write_custom_line(sockfd, "RSET")) {
		log_error(LOG_ARGS, "Could not write RSET");
		return errno;
	}

	return 0;
}
