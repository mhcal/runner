#include <sys/types.h>
#include <time.h>
#ifndef MLFQ_H
#define MLFQ_H

#define N_LEVELS 3

typedef enum { READY, RUNNING, DONE, BLOCKED } PROCESS_STATE;
typedef enum { HIGH, MEDIUM, LOW } PROCESS_LEVEL;
typedef struct process {
    pid_t pid;
    unsigned int user_id;
    time_t start_time;
    long cpu_time_last_levels;
    long cpu_time_at_level;
    long cpu_total_time;
    PROCESS_LEVEL p_level;
    PROCESS_STATE p_state;
} Process;

typedef struct node {
    Process *process;
    struct node *prev;
    struct node *next;
} Node;

typedef struct PriorityQueue {
    Node* head;
    Node* tail; // Unused pointer to further ahead implement Round Robin;
} PriorityQueue;

PriorityQueue* pq_create();
void pq_enqueue(PriorityQueue *pq, Process *p);
Process* pq_dequeue(PriorityQueue *pq);
void pq_free(PriorityQueue *pq);
Process* pq_peek(PriorityQueue *pq);

// int pq_keychange(PriorityQueue *pq, pid_t pid, int key);

// ====================================================================================================================================================

typedef struct {
    PriorityQueue *levels[N_LEVELS];
    int allotment[N_LEVELS];
} mlfq_t;

long read_cpu_ticks(pid_t pid);
void insert_new_process(mlfq_t *mlfq, pid_t pid, unsigned int user_id); // Handles a new incoming process by inserting him on the "high" PQueue;
void remove_process(mlfq_t *mlfq, Process* p);
void sync_cpu_time(Process *p);
void sync_cpu_time_level(Process *p);
void reset_cpu_time_level(Process *p);
void set_process_state(Process *p, PROCESS_STATE state);
void set_process_queue(mlfq_t *mlfq, Process *p, PROCESS_LEVEL level);

#endif
