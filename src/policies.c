#include "state.h"
#include <glib.h>

GList* fcfs(GQueue *pending) {
    if (g_queue_is_empty(pending)) return NULL;
    return g_queue_peek_head_link(pending);
}
