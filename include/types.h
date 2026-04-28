#ifndef TYPES_H
#define TYPES_H

#include <sys/types.h>
#include <stdbool.h>

#define MAX_CMDS 16
#define MAX_ARGS 64

#define CONTROLLER_FIFO "/tmp/controller_fifo"
#define RUNNER_FIFO "/tmp/runner_fifo"

typedef enum { EXECUTE, CONSULT, SHUTDOWN, FINISHED } Operation;

typedef struct {
    char *args[MAX_ARGS];
    char *in;
    char *out;
    char *err;
    bool append;
} Command;

typedef struct {
    Command cmd[MAX_CMDS];
    int num_cmds;
} Pipeline;

typedef struct {
    Operation op;
    int user_id;
    pid_t runner_pid;
} Request;

typedef struct {
    bool allowed;
    char status[1024];
} Response;

typedef struct {
    struct timeval total_time;
    int finished_processes;
} UserStats;

#endif
