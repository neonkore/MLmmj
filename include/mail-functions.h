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

#ifndef MAIL_FUNCTIONS_H
#define MAIL_FUNCTIONS_H

#define WRITE_BUFSIZE 1024

#include <stdio.h>

int write_helo(int sockfd, const char *hostname);
int write_mail_from(int sockfd, const char *from_addr);
int write_rcpt_to(int sockfd, const char *rcpt_addr);
int write_custom_line(int sockfd, const char *line);
int write_mailbody_from_map(int sockfd, char *mailmap, size_t mailsize);
int write_replyto(int sockfd, const char *replyaddr);
int write_dot(int sockfd);
int write_quit(int sockfd);
int write_data(int sockfd);
int write_rset(int sockfd);

#endif /* MAIL_FUNCTIONS_H */
