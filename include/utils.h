#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>
#include <string.h>
#include <fcntl.h>

inline static void printerr(char *s) {
    write(STDERR_FILENO, s, strlen(s));
}

#endif
