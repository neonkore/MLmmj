/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include "find_email_adr.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

struct email_container *find_email_adr(const char *str,
		struct email_container *retstruct)
{
	size_t len;
	char *index_atsign;
	char *tempstr;
	char *c, *first_char = 0, *last_char = 0;
	
	tempstr = malloc(strlen(str) + 1);
	snprintf(tempstr, strlen(str) + 1, "%s", str);

	retstruct->emailcount = 0;
	retstruct->emaillist = NULL;
	
	while((index_atsign = index(tempstr, '@'))) {
		c = index_atsign;
		retstruct->emailcount++;
		while(*c != '<' && *c != ' ' && *c != ',' && *c != ';'
				&& *c != '(' && *c != '[' && *c >= 32
				&& *c != '{' && c != tempstr) {
			c--;
		}
		first_char = ++c;
		c = index_atsign;
		while(*c != '>' && *c != ' ' && *c != ',' && *c != ';'
				&& *c != ')' && *c != ']' && *c >= 32
				&& *c != '}' && *c != 0) {
			c++;
		}
		*c = 0;
		last_char = --c;

		len = last_char - first_char + 2;
		
		retstruct->emaillist = realloc(retstruct->emaillist,
				sizeof(char **) * retstruct->emailcount);
		retstruct->emaillist[retstruct->emailcount-1] =
			malloc(len);
		snprintf(retstruct->emaillist[retstruct->emailcount-1], len,
			 "%s", first_char);
#if 0
		printf("find_email_adr, [%s]\n",
				retstruct->emaillist[retstruct->emailcount-1]);
#endif
		*index_atsign = 'A'; /* Clear it so we don't find it again */
	}
	free(tempstr);
	return retstruct;
}
