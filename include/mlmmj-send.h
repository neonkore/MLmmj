/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef MMJML_SEND_H
#define MMJML_SEND_H

int send_mail(int sockfd, const char *from, const char *to,
	      const char *replyto, FILE *mailfile);
int send_mail_many(int sockfd, const char *from, const char *replyto,
		   FILE *mailfile, FILE *subfile, const char *listaddr,
		   const char *archivefilename);
int initsmtp(int *sockfd, const char *relayhost);
int endsmtp(int *sockfd);

#endif /* MMJML_SEND_H */
