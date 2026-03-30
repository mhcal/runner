#ifndef TYPES_H
#define TYPES_H

#include <sys/types.h>

#define CMD_LEN 256

typedef enum { EXECUTE, CONSULT, SHUTDOWN } Operation;

typedef struct {
    Operation op;
    int user_id;
    pid_t runner_pid;
    char cmd[CMD_LEN];
} Request;

typedef struct {
    int allowed;
    char status[1024];
} Response;

#endif
