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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <inttypes.h>

#include "init_sockfd.h"
#include "log_error.h"

void init_sockfd(int *sockfd, const char *relayhost, unsigned short port)
{
	int len;
	struct sockaddr_in addr;

	if (getenv("MLMMJ_TESTING")) {
		relayhost = "127.0.0.1";
		port = 10025;
	}

	*sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(*sockfd == -1) {
		log_error(LOG_ARGS, "Could not get socket");
		exit(EXIT_FAILURE);
	}
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr(relayhost);
	addr.sin_port = htons(port);
	len = sizeof(addr);
	if(connect(*sockfd, (struct sockaddr *)&addr, len) == -1) {
		log_error(LOG_ARGS, "Could not connect to %s, "
				    "exiting ... ", relayhost);
		exit(EXIT_FAILURE);
	}
}
