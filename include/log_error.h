/* Copyright (C) 2004 Mads Martin Joergensen <mmj at mmj.dk>
 *
 * $Id$
 *
 * This file is redistributable under version 2 of the GNU General
 * Public License as described at http://www.gnu.org/licenses/gpl.txt
 */

#ifndef LOG_ERROR_H
#define LOG_ERROR_H

#define log_error(msg) log_error_do(msg,__FILE__,__LINE__)

void log_set_name(const char *name);
void log_error_do(const char *msg, const char *file, int line);

#endif /* LOG_ERROR_H */
