/* Copyright (C) 2002, 2003 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef FIND_EMAIL_ADR_H
#define FIND_EMAIL_ADR_H

#include <stddef.h>

struct email_container {
	int emailcount;
	char **emaillist;
};

struct email_container *find_email_adr(const char *str,
		struct email_container *retval);

#endif /* FIND_EMAIL_ADR_H */
