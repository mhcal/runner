#include "types.h"
#include "state.h"
#include "policies.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <glib.h>

void send_response(pid_t runner_pid, Response *response) {
    char runner_fifo[256];
    snprintf(runner_fifo, sizeof(runner_fifo), RUNNER_FIFO "_%d", runner_pid);

    int fd = open(runner_fifo, O_WRONLY);
    if (fd == -1)
        perror("[controller] failed to open runner FIFO");
    else {
        write(fd, response, sizeof(Response));
        close(fd);
    }
}

void dispatch(State *state) {
    while (state->current_running < state->max_parallel && !g_queue_is_empty(state->pending)) {
        // escolhemos a proxima tarefa usando a politica de escalonamento escolhida
        GList *node = state->policy(state);
        if (node == NULL) break;

        // movemos a tarefa escolhida da fila de pendentes para fila de ativos
        Task *task = (Task *)node->data;
        g_queue_delete_link(state->pending, node);
        gettimeofday(&task->start_time, NULL);
        g_queue_push_tail(state->running, task);
        state->current_running++;

        // atualizamos as estatísticas do usuário
        UserStats *stats = g_hash_table_lookup(state->users, GINT_TO_POINTER(task->request.user_id));
        if (!stats) {
            stats = g_malloc0(sizeof(UserStats));
            g_hash_table_insert(state->users, GINT_TO_POINTER(task->request.user_id), stats);
        }
        state->global_time++;
        stats->last_scheduled = state->global_time;

        // notificamos o runner
        Response response;
        memset(&response, 0, sizeof(Response));
        response.allowed = 1;
        send_response(task->request.runner_pid, &response);

        char msg[256];
        snprintf(msg, sizeof(msg), "[controller] approved execute request for command with pid %d\n", task->request.runner_pid);
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

void handle_execute(State *state, const Request *request) {
    if (request->op != EXECUTE) return;

    // formatamos o pedido na struct da tarefa e colocamos na fila de pendentes
    Task *task = g_malloc(sizeof(Task));
    memcpy(&task->request, request, sizeof(Request));
    gettimeofday(&task->submitted_time, NULL);
    g_queue_push_tail(state->pending, task);

    // tentamos despachar a proxima tarefa
    dispatch(state);
}

unsigned long long timeval_to_ms(struct timeval time) {
    return (unsigned long long)time.tv_sec * 1000ull + (unsigned long long)time.tv_usec / 1000ull;
}

void handle_finished(State *state, const Request *request) {
    if (request->op != FINISHED) return;

    for (GList *node = state->running->head; node != NULL; node = node->next) {
        Task *task = (Task *)node->data;

        if (task->request.runner_pid == request->runner_pid) {
            gettimeofday(&task->end_time, NULL);

            unsigned long long duration_submitted = timeval_to_ms(task->end_time) - timeval_to_ms(task->submitted_time);
            unsigned long long duration_start = timeval_to_ms(task->end_time) - timeval_to_ms(task->start_time);

            // computar novo nível de prioridade para o usuário
            UserStats *stats = g_hash_table_lookup(state->users, GINT_TO_POINTER(request->runner_pid));
            if (!stats) {
                stats = g_malloc0(sizeof(UserStats));
                stats->last_scheduled = state->global_time;
                g_hash_table_insert(state->users, GINT_TO_POINTER(request->user_id), stats);
            }

            stats->total_time += duration_start;
            if (stats->total_time > LOW_PRIORITY_THRESHOLD) stats->priority = LOW;
            else if (stats->total_time > MEDIUM_PRIORITY_THRESHOLD) stats->priority = MEDIUM;
            else stats->priority = HIGH;

            // grava a entrada no ficheiro persistente
            char entry[256];
            snprintf(entry, sizeof(entry), "user_id: %d | pid: %d | duration: %llu ms\n",
                     request->user_id, request->runner_pid, duration_submitted);

            char log[64];
            snprintf(log, sizeof(log), "tmp/execution_log_%d.txt", getpid()); // talvez mudar isso para garantirmos unicidade (?)

            int fd = open(log, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd != -1) {
                write(fd, entry, strlen(entry));
                close(fd);
            } else
                perror("[controller] failed to open log file");

            // cleanup
            g_queue_delete_link(state->running, node);
            g_free(task);
            state->current_running--;
            break;
        }
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "[controller] command with pid %d finished\n", request->runner_pid);
    write(STDOUT_FILENO, msg, strlen(msg));

    // acabamos de liberar um espaço; tentamos despachar a proxima tarefa
    dispatch(state);
}

void handle_consult(State *state, const Request *request, Response *response) {
    if (request->op != CONSULT) return;

    response->allowed = true;
    memset(response->status, 0, sizeof(response->status));
    char buffer[256];

    strcat(response->status, "---\nExecuting\n");
    for (GList *node = state->running->head; node != NULL; node = node->next) {
        Task *task = (Task *)node->data;
        snprintf(buffer, sizeof(buffer), "user-id %d - command-pid %d\n", task->request.user_id, task->request.runner_pid);
        strcat(response->status, buffer);
    }

    strcat(response->status, "---\nScheduled\n");
    for (GList *node = state->pending->head; node != NULL; node = node->next) {
        Task *task = (Task *)node->data;
        snprintf(buffer, sizeof(buffer), "user-id %d - command-pid %d\n", task->request.user_id, task->request.runner_pid);
        strcat(response->status, buffer);
    }
}

void handle_shutdown(State *state, const Request *request, Response *response) {
    if (request->op != SHUTDOWN) return;

    if (!state->on || state->shutdown_pid > 0) {
        response->allowed = false;
        snprintf(response->status, sizeof(response->status), "Shutdown already in progress\n");
        return;
    }

    state->on = false;
    state->shutdown_pid = request->runner_pid; // guardamos o pid do usuário para avisarmos ao fim
    response->allowed = true;

    char msg[256];
    snprintf(msg, sizeof(msg), "[controller] shutdown request from %d\n", request->runner_pid);
    write(STDOUT_FILENO, msg, strlen(msg));
}

// retorna 1 para manter o controller ativo, 0 para terminar
void handle_request(State *state, const Request *request) {
    int need_response = 1;
    Response response;
    memset(&response, 0, sizeof(Response));

    switch(request->op) {
        case EXECUTE:
            if (state->on) {
                handle_execute(state, request);
                need_response = 0; // dispatch é responsavel por notificar o runner
            } else {
                response.allowed = false;
                snprintf(response.status, sizeof(response.status), "Controller is shutting down\n");
            }
            break;
        case CONSULT:
            handle_consult(state, request, &response);
            break;
        case SHUTDOWN:
            handle_shutdown(state, request, &response);
            break;
        case FINISHED:
            handle_finished(state, request);
            need_response = 0;
            break;
        default:
            response.allowed = 0;
            snprintf(response.status, sizeof(response.status), "Unrecognized operation\n");
            break;
    }

    if (need_response) send_response(request->runner_pid, &response);
}

int main(int argc, char *argv[]) {
    if (argc != 3 || atoi(argv[1]) <= 0) {
        printerr("Error. Usage: ./controller [parallel-commands] [sched-policy]\n");
        return 1;
    }

    State state;
    state.on = true;
    state.max_parallel = atoi(argv[1]);
    state.current_running = 0;
    state.pending = g_queue_new();
    state.running = g_queue_new();
    state.users = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    state.global_time = 0;
    state.shutdown_pid = 0;

    char *sched_policy = argv[2];

    if (strcmp(sched_policy, "fcfs") == 0) state.policy = fcfs;
    else if (strcmp(sched_policy, "rr") == 0) state.policy = rr;
    else if (strcmp(sched_policy, "mlfq") == 0) state.policy = mlfq;
    else {
        printerr("Error. Unknown scheduling policy.\n");
        return 1;
    }

    unlink(CONTROLLER_FIFO); // fechar pipes de execucoes anteriores

    if (mkfifo(CONTROLLER_FIFO, 0666) == -1) {
        perror("[controller] failed to create controller FIFO");
        return 1;
    }

    int fd = open(CONTROLLER_FIFO, O_RDONLY);
    if (fd == -1) {
        perror("[controller] failed to open controller FIFO");
        return 1;
    }

    Request request;

    while (state.on || state.current_running > 0 || !g_queue_is_empty(state.pending)) {
        ssize_t bytes_read = read(fd, &request, sizeof(Request));

        if (bytes_read == sizeof(Request)) {
            handle_request(&state, &request);
        }

        else if (bytes_read == 0) {
            close(fd);
            if (!state.on && state.current_running == 0 && g_queue_is_empty(state.pending)) break;

            fd = open(CONTROLLER_FIFO, O_RDONLY);
            if (fd == -1) {
                perror("[controller] failed to reopen controller FIFO");
                break;
            }
        }

        else perror("[controller] error reading from FIFO;");
    }

    // avisar o controller que pediu o shutdown
    if (state.shutdown_pid > 0) {
        Response bye;
        memset(&bye, 0, sizeof(Response));
        bye.allowed = true;
        send_response(state.shutdown_pid, &bye);
    }

    // cleanup
    close(fd);
    unlink(CONTROLLER_FIFO);
    g_queue_free_full(state.pending, g_free);
    g_queue_free_full(state.running, g_free);
    g_hash_table_destroy(state.users);

    return 0;
}
