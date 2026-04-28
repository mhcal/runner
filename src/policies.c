#include "state.h"
#include "policies.h"
#include <glib.h>

GList* fcfs(gpointer arg) {
    State *state = (State *)arg;
    if (g_queue_is_empty(state->pending)) return NULL;
    return g_queue_peek_head_link(state->pending);
}

GList* rr(gpointer arg) {
    State *state = (State *)arg;
    if (g_queue_is_empty(state->pending)) return NULL;
    gconstpointer user_id = (gconstpointer)g_tree_node_value(g_tree_node_first(state->policy_tree));

    return g_queue_find_custom(state->pending, user_id, find_task_by_id);
}