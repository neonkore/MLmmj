#ifndef DO_ALL_THE_VOODO_HERE_H
#define DO_ALL_THE_VOODO_HERE_H

#include "mlmmj.h"

int findit(const char *line, const char **headers);
void getinfo(const char *line, struct mailhdr *readhdrs);
void do_all_the_voodo_here(FILE *in, FILE *out, FILE *hdradd, FILE *footers,
	      const char **delhdrs, struct mailhdr *readhdrs,
	      const char *subjectprefix);

#endif /* DO_ALL_THE_VOODO_HERE_H */
