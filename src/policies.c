#include "state.h"
#include <glib.h>
#include <limits.h>

GList* fcfs(State *state) {
    if (g_queue_is_empty(state->pending)) return NULL;
    return g_queue_peek_head_link(state->pending);
}

GList* rr(State *state) {
    if (g_queue_is_empty(state->pending)) return NULL;

    GList *pick = state->pending->head;
    unsigned long long best = ULLONG_MAX;

    for (GList *node = state->pending->head; node != NULL; node = node->next) {
        Task *task = (Task *)node->data;
        UserStats *stats = g_hash_table_lookup(state->users, GINT_TO_POINTER(task->request.user_id));

        unsigned long long curr = stats ? stats->last_scheduled : 0; // se o usuário ainda não tem entradas na HashTable, então não correu comandos

        if (best == ULLONG_MAX || curr < best) {
            best = curr;
            pick = node;
        }
    }

    return pick;
}

GList* mlfq(State *state) {
    if (g_queue_is_empty(state->pending)) return NULL;

    GList *pick = state->pending->head;
    Priority best = LOW + 1;

    for (GList *node = state->pending->head; node != NULL; node = node->next) {
        Task *task = (Task *)node->data;
        UserStats *stats = g_hash_table_lookup(state->users, GINT_TO_POINTER(task->request.user_id));

        Priority curr = stats ? stats->priority : HIGH; // mesma lógica que o curr do rr

        if (curr < best) {
            best = curr;
            pick = node;
            if (best == HIGH) break; // desempate por ordem de chegada
        }
    }

    return pick;
}
