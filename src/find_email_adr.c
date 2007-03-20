/* Some crap from BSD mail to parse email addresses.
 *
 * Neale Pickett <neale@woozle.org> stole these functions from FreeBSD
 * ports, and reformatted them to use mlmmj's coding style.  The
 * original functions (skip_comment and skin) had the following
 * copyright notice:
 */

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "find_email_adr.h"
#include "memory.h"

/*
 * Start of a "comment".
 * Ignore it.
 */
static char *
skip_comment(char *cp)
{
	int nesting = 1;

	for (; nesting > 0 && *cp; cp++) {
		switch (*cp) {
			case '\\':
				if (cp[1])
					cp++;
				break;
			case '(':
				nesting++;
				break;
			case ')':
				nesting--;
				break;
		}
	}
	return (cp);
}

/*
 * Skin an arpa net address according to the RFC 822 interpretation
 * of "host-phrase."
 */
static char *skin(char *name)
{
	char *nbuf, *bufend, *cp, *cp2;
	int c, gotlt, lastsp;

	if (name == NULL)
		return (NULL);

	/* We assume that length(input) <= length(output) */
	nbuf = mymalloc(strlen(name) + 1);

	if (strchr(name, '(') == NULL && strchr(name, '<') == NULL
	    && strchr(name, ' ') == NULL) {
		strcpy(nbuf, name);
		return (nbuf);
	}

	gotlt = 0;
	lastsp = 0;
	bufend = nbuf;
	for (cp = name, cp2 = bufend; (c = *cp++) != '\0'; ) {
		switch (c) {
		case '(':
			cp = skip_comment(cp);
			lastsp = 0;
			break;

		case '"':
			/*
			 * Start of a "quoted-string".
			 * Copy it in its entirety.
			 */
			while ((c = *cp) != '\0') {
				cp++;
				if (c == '"')
					break;
				if (c != '\\')
					*cp2++ = c;
				else if ((c = *cp) != '\0') {
					*cp2++ = c;
					cp++;
				}
			}
			lastsp = 0;
			break;

		case ' ':
			if (cp[0] == 'a' && cp[1] == 't' && cp[2] == ' ')
				cp += 3, *cp2++ = '@';
			else
			if (cp[0] == '@' && cp[1] == ' ')
				cp += 2, *cp2++ = '@';
			else
				lastsp = 1;
			break;

		case '<':
			cp2 = bufend;
			gotlt++;
			lastsp = 0;
			break;

		case '>':
			if (gotlt) {
				gotlt = 0;
				while ((c = *cp) != '\0' && c != ',') {
					cp++;
					if (c == '(')
						cp = skip_comment(cp);
					else if (c == '"')
						while ((c = *cp) != '\0') {
							cp++;
							if (c == '"')
								break;
							if (c == '\\' && *cp != '\0')
								cp++;
						}
				}
				lastsp = 0;
				break;
			}
			/* FALLTHROUGH */

		default:
			if (lastsp) {
				lastsp = 0;
				*cp2++ = ' ';
			}
			*cp2++ = c;
			if (c == ',' && *cp == ' ' && !gotlt) {
				*cp2++ = ' ';
				while (*++cp == ' ')
					;
				lastsp = 0;
				bufend = cp2;
			}
		}
	}
	*cp2 = '\0';

	nbuf = (char *)myrealloc(nbuf, strlen(nbuf) + 1);
	return (nbuf);
}


struct email_container *find_email_adr(const char *str,
		struct email_container *retstruct)
{
	char *c1 = NULL, *c2 = NULL;
	char *p;
	char *s;

	s = (char *)mymalloc(strlen(str) + 1);
	strcpy(s, str);

	p = s;
	while(p) {
		char *adr;
		char *cur;

		cur = p;
oncemore:
		p = strchr(p, ',');
		if (p) {
			/* If there's a comma, replace it with a NUL, so
			 * cur will only have one address in it. Except
			 * it's not in ""s */
			c1 = strchr(cur, '"');
			if(c1) {
				c2 = strchr(c1+1, '"');
			}
			if(c2) {
				if(*(c2-1) == '\\') {
					*c2 = ' ';
					c2 = NULL;
					goto oncemore;
				}
			}
			if((c1 == NULL) || (c1 > p) || (c2 && c2 < p)) {
				*p = '\0';
				p += 1;
			} else {
				*p = ' ';
				goto oncemore;
			}
		}

		while(cur && ((' ' == *cur) ||
			    ('\t' == *cur) ||
			    ('\r' == *cur) ||
			    ('\n' == *cur))) {
			cur += 1;
		}
		if ('\0' == *cur) {
			continue;
		}

		adr = skin(cur);
		if (adr) {
			retstruct->emailcount++;
			retstruct->emaillist = (char **)myrealloc(retstruct->emaillist,
					  sizeof(char *) * retstruct->emailcount);
			retstruct->emaillist[retstruct->emailcount-1] = adr;
		}
	}

	myfree(s);

	return retstruct;
}
