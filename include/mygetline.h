#ifndef MYGETLINE_H
#define MYGETLINE_H
#include <stdio.h>

#define BUFSIZE 256

char *myfgetline(FILE *infile);
char *mygetline(int fd);

#endif /* #ifndef MYGETLINE_H */
