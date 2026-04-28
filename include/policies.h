#ifndef POLICIES_H
#define POLICIES_H

#include "state.h"
#include <glib.h>

static gint find_task_by_id(gconstpointer target, gconstpointer id) {
    const Task *task = (const Task*)target;
    return task->request.user_id - *(int *)id;
}

GList* fcfs(gpointer arg);
GList* rr(gpointer arg);

#endif
