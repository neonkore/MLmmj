/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "init_sockfd.h"
#include "log_error.h"

void init_sockfd(int *sockfd, const char *relayhost)
{
	int len;
	struct sockaddr_in addr;

	*sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(*sockfd == -1) {
		log_error(LOG_ARGS, "Could not get socket");
		exit(EXIT_FAILURE);
	}
	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr(relayhost);
	addr.sin_port = htons(25);
	len = sizeof(addr);
	if(connect(*sockfd, (struct sockaddr *)&addr, len) == -1) {
		log_error(LOG_ARGS, "Could not connect");
		exit(EXIT_FAILURE);
	}
}

