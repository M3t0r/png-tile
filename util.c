#include "util.h" // check for signature mismatch

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <curses.h>
#include <term.h>

int term_is_interactive;
int term_columns;

void fancy_init() {
    term_is_interactive = isatty(STDOUT_FILENO);
    if(term_is_interactive) {
        setterm(NULL);
        term_columns = tgetnum("cols");
        if(term_columns < 0) term_columns = 80;
    }
}

void progress_meter(char *job, double progress) {
    if(term_is_interactive) {
        int pbar_length = term_columns - strlen(job) - 13;
        int pbar_num_full = ceil(pbar_length*progress);

        char *pbar = malloc(pbar_length + 1);
        memset(pbar, '#', pbar_num_full);
        memset(pbar+pbar_num_full, ' ', pbar_length - pbar_num_full);
        pbar[pbar_length] = 0;

        printf("%s: [%s] % 3.2f%%\r", job, pbar, progress*100);
        fflush(stdout);

        free(pbar);
    }
}

int perr(const char *format, ...) {
    va_list va_list;
    va_start(va_list, format);
    vfprintf(stderr, format, va_list);
    va_end(va_list);
    return EXIT_FAILURE;
}

void *salloc(size_t num_bytes) {
    void *r = malloc(num_bytes);

    if(r == NULL) {
        exit(perr("Could not allocate a small memory block.\n"));
    }

    return r;
}
