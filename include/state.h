#ifndef STATE_H
#define STATE_H

#include "types.h"
#include <glib.h>
#include <sys/time.h>
#include <stdbool.h>

// baixa (> 30s), média (10-30s), alta (< 10s)
#define LOW_PRIORITY_THRESHOLD 30000ULL
#define MEDIUM_PRIORITY_THRESHOLD 10000ULL

typedef struct {
    Request request;
    struct timeval submitted_time;
    struct timeval start_time;
    struct timeval end_time;
} Task;

typedef enum { HIGH = 0, MEDIUM, LOW } Priority;

typedef struct {
    Priority priority;
    unsigned long long total_time;
    unsigned long long last_scheduled;
} UserStats;

// definimos o tipo separadamente da struct para usar na assinatura da PolicyFunction
typedef struct State_t State;

typedef GList* (*PolicyFunction)(State *state);

struct State_t {
    bool on;
    int max_parallel;
    int current_running;
    GQueue *pending;
    GQueue *running;
    PolicyFunction policy;
    GHashTable *users;
    unsigned long long global_time;
    pid_t shutdown_pid;
};

#endif
