#ifndef DO_ALL_THE_VOODO_HERE_H
#define DO_ALL_THE_VOODO_HERE_H

#include "mlmmj.h" /* For struct mailhdr */

int findit(const char *line, const char **headers);
void getinfo(const char *line, struct mailhdr *readhdrs);
int do_all_the_voodo_here(int infd, int outfd, int hdrfd, int footfd,
	      const char **delhdrs, struct mailhdr *readhdrs,
	      const char *subjectprefix);

#endif /* DO_ALL_THE_VOODO_HERE_H */
