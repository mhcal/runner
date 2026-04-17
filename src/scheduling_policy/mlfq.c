// For the sake of these functions, the caller owns the Process objects.

#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include "mlfq.h"
#include "utils.h"

PriorityQueue* pq_create() {
    PriorityQueue *queue = malloc(sizeof(struct PriorityQueue));
    if (!queue) return NULL;
    queue->head = NULL;
    queue->tail = NULL;
    return queue;
}

void pq_free(PriorityQueue *pq) {
    while(pq->head) {
        Node *tmp = pq->head;
        pq->head = tmp->next;
        free(tmp);
    }
    free(pq);
}

void pq_enqueue(PriorityQueue *pq, Process *p) {
    if (!pq) {
        printerr("Queue not initialized.");
        exit(1);
    }
    Node *new_process = malloc(sizeof(struct Node));
    if (new_process == NULL) {
        printerr("Failed malloc allocation.");
        return;
    }
    new_process->process = p;
    new_process->prev = pq->tail;
    new_process->next = NULL;

    if(pq->head == NULL) {
        pq->head = new_process;
        return;
    }

    if (pq->tail == NULL) {
        new_process->prev = pq->head;
        pq->head->next = new_process;
        pq->tail = new_process;
    } else {
        pq->tail->next = new_process;
        pq->tail = pq->tail->next;
    }
}

Process* pq_dequeue(PriorityQueue *pq) {
    if (!pq || !pq->head) return NULL;
    Node *tmp = pq->head;
    Process *p = tmp->process;

    pq->head = tmp->next;

    if (pq->head != NULL) {
        pq->head->prev = NULL;
    } else {
        pq->tail = NULL;
    }


    free(tmp);
    return p;
}

Process* pq_peek(PriorityQueue *pq) {
    if (pq == NULL) return NULL;
    if (pq->head) return pq->head->process;
    else return NULL;
}
// TODO: Fazer isso utilizando system calls.
// long read_cpu_ticks(pid_t pid);

void insert_new_process(mlfq_t *mlfq, pid_t pid, unsigned int user_id) {
    if (mlfq == NULL) {
        printerr("MLFQ not initialized.");
        exit(1);
    }

    Process *p = malloc(sizeof(struct process));
    if (p == NULL) {
        printerr("Failed malloc allocation.");
        exit(1);
    }
    p->pid = pid;
    p->user_id = user_id;
    p->start_time = time(NULL);
    if (p->start_time == (time_t)-1) {
        printerr("Couldn't compute the arriving time of the process.");
        exit(1);
    }
    p->cpu_time_last_levels = 0;
    p->cpu_time_at_level = 0;
    p->cpu_total_time = 0;
    p->p_level = HIGH;
    p->p_state = READY;

    pq_enqueue(mlfq->levels[HIGH], p);
}

void remove_process(mlfq_t *mlfq, Process *p) {
     if (mlfq == NULL) {
         printerr("Scheduler not initialized.");
         exit(1);
     }

     Node *current = mlfq->levels[p->p_level]->head;

     if (current == NULL) {
         printerr("Invalid process, process passed as argument has invalid level since the queue is empty.");
         return;
     }

     while (current) {
         if (current->process->pid == p->pid) {
             break;
         }
         current = current->next;
     }

     if (current == NULL) {
         printerr("Invalid process, since the process is marked as a member of the queue but isn't in it.");
         return;
     }

     if (current->prev != NULL) {
         current->prev->next = current->next;
     }

     if (current->next != NULL) {
         current->next->prev = current->prev;
     }

     if (mlfq->levels[p->p_level]->head == current) {
         mlfq->levels[p->p_level]->head = current->next;
     }

     if (mlfq->levels[p->p_level]->tail == current) {
         mlfq->levels[p->p_level]->tail = current->prev;
     }

     p->p_state = DONE;
     free(current);
     return;

}

void sync_cpu_time(Process *p) {
    p->cpu_total_time = read_cpu_ticks(p->pid);
}

void sync_cpu_time_level(Process *p) {
    p->cpu_time_at_level = read_cpu_ticks(p->pid) - p->cpu_time_last_levels;
}


void reset_cpu_time_level(Process *p) {
    p->cpu_time_at_level = 0;
}

void set_process_state(Process *p, PROCESS_STATE state) {
    p->p_state = state;
}

void set_process_queue(mlfq_t *mlfq, Process *p, PROCESS_LEVEL level) {
    if (mlfq == NULL) {
        printerr("MLFQ not initialized.");
        exit(1);
    }

    Node *current = mlfq->levels[p->p_level]->head;
    if (current == NULL) {
        printerr("Invalid process, process passed as argument has invalid level since the queue is empty.");
        return;
    }

    while (current) {
        if (current->process->pid == p->pid) {
            break;
        }
        current = current->next;
    }

    if (current == NULL) {
        printerr("Invalid process, since the process is marked as a member of the queue but isn't in it.");
        return;
    }
    sync_cpu_time_level(p);
    p->cpu_time_last_levels += p->cpu_time_at_level;
    reset_cpu_time_level(p);

    if (current->prev != NULL) {
        current->prev->next = current->next;
    }

    if (current->next != NULL) {
        current->next->prev = current->prev;
    }

    if (mlfq->levels[p->p_level]->head == current) {
        mlfq->levels[p->p_level]->head = current->next;
    }

    if (mlfq->levels[p->p_level]->tail == current) {
        mlfq->levels[p->p_level]->tail = current->prev;
    }

    p->p_level = level;
    free(current);
    pq_enqueue(mlfq->levels[level], p);
}
