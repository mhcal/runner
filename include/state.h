#ifndef STATE_H
#define STATE_H

#include "types.h"
#include <glib.h>
#include <sys/time.h>
#include <stdbool.h>

typedef struct {
    Request request;
    struct timeval submitted_time;
    struct timeval start_time;
    struct timeval end_time;
} Task;

typedef GList* (*PolicyFunction)(gpointer arg);

typedef struct {
    bool on;
    int max_parallel;
    int current_running;
    GQueue *pending;
    GQueue *running;
    GHashTable *user_hash_table;
    GTree *policy_tree;
    PolicyFunction policy;
} State;

#endif
