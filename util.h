#ifndef __HEADER_UTIL_H
#define __HEADER_UTIL_H

#include <stddef.h>

extern char *VERSION_STRING; // in main.c, used for metadata in generated PNGs

extern int term_is_interactive;
extern int term_columns;

void fancy_init();

void progress_meter(char *job, double progress);
int perr(const char *format, ...);
void *salloc(size_t num_bytes);

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

typedef unsigned char byte;
typedef unsigned int  uint;

#endif
