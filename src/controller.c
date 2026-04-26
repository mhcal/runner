#include "types.h"
#include "state.h"
#include "policies.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <glib.h>

void send_response(pid_t runner_pid, Response *response) {
    char runner_fifo[CMD_LEN];
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
        GList *node = state->policy(state->pending);
        if (node == NULL) break;

        // movemos a tarefa escolhida da fila de pendentes para fila de ativos
        Task *task = (Task *)node->data;
        g_queue_delete_link(state->pending, node);
        gettimeofday(&task->start_time, NULL);
        g_queue_push_tail(state->running, task);
        state->current_running++;

        // notificamos o runner
        Response response;
        memset(&response, 0, sizeof(Response));
        response.allowed = 1;
        send_response(task->request.runner_pid, &response);
        char msg[256];
        snprintf(msg, sizeof(msg), "[controller] approved execute request for command with pid %d\n", task->request.runner_pid);
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

void handle_finished(State *state, const Request *request) {
    if (request->op != FINISHED) return;

    for (GList *node = state->running->head; node != NULL; node = node->next) {
        Task *task = (Task *)node->data;

        if (task->request.runner_pid == request->runner_pid) {
            gettimeofday(&task->end_time, NULL);

            // TODO: colocar a tarefa terminada no arquivo persistente
            // possivelmente computar um novo nivel de prioridade para uma tabela de estatisticas de usuario (mlfq)

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

    response->allowed = 1;
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

int handle_shutdown(const Request *request, Response *response) {
    if (request->op != SHUTDOWN) return 1;

    // TODO: esperar que processos atuais terminem (por enquanto apenas paramos o loop do controller)
    response->allowed = 1;
    return 0;
}

// retorna 1 para manter o controller ativo, 0 para terminar
int handle_request(State *state, const Request *request) {
    int need_response = 1;
    Response response;
    memset(&response, 0, sizeof(Response));
    int ret = 1;

    switch(request->op) {
        case EXECUTE:
            handle_execute(state, request);
            need_response = 0; // dispatch é responsavel por notificar o runner
            break;
        case CONSULT:
            handle_consult(state, request, &response);
            break;
        case SHUTDOWN:
            ret = handle_shutdown(request, &response);
            break;
        case FINISHED:
            handle_finished(state, request);
            need_response = 0;
            break;
        default:
            response.allowed = 0;
            snprintf(response.status, sizeof(response.status), "?");
            break;
    }

    if (need_response) send_response(request->runner_pid, &response);
    return ret;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printerr("Error. Usage: ./controller [parallel-commands] [sched-policy]\n");
        return 1;
    }

    State state;
    state.max_parallel = atoi(argv[1]);
    state.current_running = 0;
    state.pending = g_queue_new();
    state.running = g_queue_new();

    char *sched_policy = argv[2];
    if (strcmp(sched_policy, "fcfs") == 0) {
        state.policy = fcfs;
    } else {
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
    int running = 1;

    while (running) {
        ssize_t bytes_read = read(fd, &request, sizeof(Request));
        if (bytes_read == sizeof(Request)) {
            running = handle_request(&state, &request);
        }

        else if (bytes_read == 0) {
            // nao tenho muita certeza do que fazer aqui; a principio, tentamos reabrir a FIFO (?)
            close(fd);

            fd = open(CONTROLLER_FIFO, O_RDONLY);
            if (fd == -1) {
                perror("[controller] failed to reopen controller FIFO");
                break;
            }
        }

        else
            perror("[controller] error reading from FIFO");
    }

    // cleanup
    close(fd);
    unlink(CONTROLLER_FIFO);
    g_queue_free_full(state.pending, g_free);
    g_queue_free_full(state.running, g_free);

    return 0;
}
