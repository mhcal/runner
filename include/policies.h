#ifndef POLICIES_H
#define POLICIES_H

#include "state.h"
#include <glib.h>

GList* fcfs(State *state);
GList* rr(State *state);
GList* mlfq(State *state);

#endif
