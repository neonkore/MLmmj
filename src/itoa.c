/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#include <string.h>
#include "itoa.h"

void reversestr(char *str)
{
	int c, i, j;

	for(i = 0, j = strlen(str) - 1; i < j; i++, j--) {
		c = str[i];
		str[i] = str[j];
		str[j] = c;
	}
}

void itoa(int n, char *str)
{
	int i, sign;

	if((sign = n) < 0)
		n = -n;
	i = 0;
	do {
		str[i++] = n % 10 + '0';
	} while((n /= 10) > 0);
	if(sign < 0)
		str[i++] = '-';
	str[i] = 0;
	reversestr(str);
}
