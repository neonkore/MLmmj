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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>

#include "mail-functions.h"
#include "wrappers.h"
#include "log_error.h"
#include "memory.h"

/* "HELO \r\n " has length 7 */
#define EXTRA_HELO_LEN 8
int write_helo(int sockfd, const char *hostname)
{
	size_t len = (size_t)(strlen(hostname) + EXTRA_HELO_LEN);
	char *helo;
	size_t bytes_written;
	
	if((helo = mymalloc(len)) == 0)
		return errno;
	snprintf(helo, len, "HELO %s\r\n", hostname);
	len = strlen(helo);
#if 0
	fprintf(stderr, "\nwrite_helo, helo = [%s]\n", helo);
#endif
	bytes_written = writen(sockfd, helo, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write HELO");
		myfree(helo);
		return errno;
	}
	myfree(helo);
	return 0;
}
/* "MAIL FROM: <>\r\n" has length 15 */
#define EXTRA_FROM_LEN 16
int write_mail_from(int sockfd, const char *from_addr)
{
	size_t len = (size_t)(strlen(from_addr) + EXTRA_FROM_LEN);
	char *mail_from;
	size_t bytes_written;

	if((mail_from = mymalloc(len)) == NULL)
		return errno;
	snprintf(mail_from, len, "MAIL FROM: <%s>\r\n", from_addr);
	len = strlen(mail_from);

#if 0
	fprintf(stderr, "\nwrite_mail_from, mail_from = [%s]\n", mail_from);
#endif
	bytes_written = writen(sockfd, mail_from, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write FROM");
		myfree(mail_from);
		return errno;
	}
	myfree(mail_from);
	return 0;
}

/* "RCPT TO: <>\r\n" has length 13 */
#define EXTRA_RCPT_LEN 14

int write_rcpt_to(int sockfd, const char *rcpt_addr)
{
	size_t len = (size_t)(strlen(rcpt_addr) + EXTRA_RCPT_LEN);
	char *rcpt_to;
	size_t bytes_written;
	
	if((rcpt_to = mymalloc(len)) == 0)
		return errno;

	snprintf(rcpt_to, len, "RCPT TO: <%s>\r\n", rcpt_addr);
	len = strlen(rcpt_to);

#if 0
	fprintf(stderr, "\nwrite_rcpt_to, rcpt_to = [%s]\n", rcpt_to);
#endif
	bytes_written = writen(sockfd, rcpt_to, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write TO");
		myfree(rcpt_to);
		return errno;
	}
	myfree(rcpt_to);
	return 0;
}


int write_mailbody_from_map(int sockfd, char *mapstart, size_t size,
			    const char *tohdr)
{
	char *cur, *next;
	char newlinebuf[3];
	size_t len;
	int i = 1;

	for(next = cur = mapstart; next < mapstart + size; next++) {
		if(*next == '\n') {
			if(writen(sockfd, cur, next - cur) < 0) {
				log_error(LOG_ARGS, "Could not write mail");
				return -1;
			}
			newlinebuf[0] = '\r';
			newlinebuf[1] = '\n';
			len = 2;
			if(*(next+1) == '.') {
				newlinebuf[2] = '.';
				len = 3;
			}
			if(writen(sockfd, newlinebuf, len) < 0) {
				log_error(LOG_ARGS, "Could not write mail");
				return -1;
			}
			if(i && tohdr && *(next+1) == '\n') { /* add To: header */
				if(writen(sockfd, tohdr, strlen(tohdr)) < 0) {
					log_error(LOG_ARGS, "Could not write"
							    " To: header");
					return -1;
				}
				i = 0; /* Make sure we don't write it again */
			}
			cur = next + 1;
		}
	}

	return 0;
}

char *get_preppedhdrs_from_map(char *mapstart, size_t *hlen)
{
	char *cur, *next, *endhdrs, *retstr, *r;
	const char newlinebuf[] = "\r\n";
	size_t hdrlen, n = 0;

	endhdrs = strstr(mapstart, "\n\n");
	if(endhdrs == NULL)
		return NULL; /* The map doesn't map a file with a mail */

	hdrlen = endhdrs - mapstart + 1;

	for(next = cur = mapstart; next < mapstart + hdrlen; next++)
		if(*next == '\n')
			n++;

	retstr = mymalloc(hdrlen + n);
	*hlen = hdrlen + n;	
	r = retstr;	

	for(next = cur = mapstart; next < mapstart + hdrlen; next++) {
		if(*next == '\n') {
			strncpy(r, cur, next - cur);
			r += next - cur;
			strncpy(r, newlinebuf, 2);
			r += 2;
			cur = next + 1;
		}
	}

	return retstr;
}

char *get_prepped_mailbody_from_map(char *mapstart, size_t size, size_t *blen)
{
	char *cur, *next, *endhdrs, *retstr, *r;
	char newlinebuf[3];
	size_t bodylen, len, n = 0;

	endhdrs = strstr(mapstart, "\n\n");
	if(endhdrs == NULL)
		return NULL; /* The map doesn't map a file with a mail */

	endhdrs++; /* Skip the first newline, it's in hdrs */

	bodylen = size - (endhdrs - mapstart);
	
	for(next = cur = endhdrs; next < mapstart + size; next++) {
		if(*next == '\n') {
			n++;
			if(*(next+1) == '.')
				n++;
		}
	}

	retstr = mymalloc(bodylen + n);
	*blen = bodylen + n;
	r = retstr;	
	
	for(next = cur = endhdrs; next < mapstart + size; next++) {
		if(*next == '\n') {
			strncpy(r, cur, next - cur);
			r += next - cur;
			newlinebuf[0] = '\r';
			newlinebuf[1] = '\n';
			len = 2;
			if(*(next+1) == '.') {
				newlinebuf[2] = '.';
				len = 3;
			}
			strncpy(r, newlinebuf, len);
			r += len;
			cur = next + 1;
		}
	}

	return retstr;
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
	
	if((customline = mymalloc(len)) == 0)
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
		myfree(customline);
		return errno;
	}
	myfree(customline);
	return 0;
}

/* "Reply-To: \r\n" has length 12 */
#define EXTRA_REPLYTO_LEN 13

int write_replyto(int sockfd, const char *replyaddr)
{
	size_t len = (size_t)(strlen(replyaddr) + EXTRA_REPLYTO_LEN);
	char *replyto;
	size_t bytes_written;
	
	if((replyto = mymalloc(len)) == 0)
		return errno;

	snprintf(replyto, len, "Reply-To: %s\r\n", replyaddr);
	len = strlen(replyto);

#ifdef MLMMJ_DEBUG
	fprintf(stderr, "\nwrite_replyto, replyto = [%s]\n", replyto);
#endif
	bytes_written = writen(sockfd, replyto, len);
	if(bytes_written < 0) {
		log_error(LOG_ARGS, "Could not write Reply-To header");
		myfree(replyto);
		return errno;
	}
	myfree(replyto);
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
