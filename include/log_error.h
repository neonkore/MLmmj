/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef LOG_ERROR_H
#define LOG_ERROR_H

#include <errno.h>

#define LOG_ARGS __FILE__, __LINE__, strerror(errno)

void log_set_name(const char *name);
void log_free_name(void);
void log_error(const char *file, int line, const char *errstr,
		const char *fmt, ...);

#endif /* LOG_ERROR_H */
