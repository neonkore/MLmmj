/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef MAIL_FUNCTIONS_H
#define MAIL_FUNCTIONS_H

#define WRITE_BUFSIZE 1024

#include <stdio.h>

int write_helo(int sockfd, const char *hostname);
int write_mail_from(int sockfd, const char *from_addr);
int write_rcpt_to(int sockfd, const char *rcpt_addr);
int write_custom_line(int sockfd, const char *line);
int write_mailbody_from_fd(int sockfd, int fd);
int write_replyto(int sockfd, const char *replyaddr);
int write_dot(int sockfd);
int write_quit(int sockfd);
int write_data(int sockfd);
int write_rset(int sockfd);

#endif /* MAIL_FUNCTIONS_H */
