#include "types.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

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

void handle_finished(const Request *request) {
    if (request->op != FINISHED) return;

    // for testing purposes
    char msg[256];
    snprintf(msg, sizeof(msg), "[controller] command with pid %d finished\n", request->runner_pid);
    write(STDOUT_FILENO, msg, strlen(msg));
}

void handle_execute(const Request *request, Response *response) {
    if (request->op != EXECUTE) return;

    // TODO: verificar numero maximo de processos; implementar politicas de escalonamento
    response->allowed = 1;

    // for testing purposes
    char msg[256];
    snprintf(msg, sizeof(msg), "[controller] execute request for command with pid %d approved\n", request->runner_pid);
    write(STDOUT_FILENO, msg, strlen(msg));
}

void handle_consult(const Request *request, Response *response) {
    if (request->op != EXECUTE) return;

    // TODO: formatar processos que estao a correr em `request->status`
    response->allowed = 1;
}

int handle_shutdown(const Request *request, Response *response) {
    if (request->op != SHUTDOWN) return 1;

    // TODO: esperar que processos atuais terminem (por enquanto apenas paramos o loop do controller)
    response->allowed = 1;
    return 0;
}

// retorna 1 para manter o controller ativo, 0 para terminar
int handle_request(const Request *request) {
    int need_response = 1;
    Response response;
    memset(&response, 0, sizeof(Response));
    int ret = 1;

    switch(request->op) {
        case EXECUTE:
            handle_execute(request, &response);
            break;
        case CONSULT:
            handle_consult(request, &response);
            break;
        case SHUTDOWN:
            ret = handle_shutdown(request, &response);
            break;
        case FINISHED:
            handle_finished(request);
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

    // TODO: provavelmente teremos que criar uma state struct pra guardar e consultar informacao como essa
    int max_parallel = atoi(argv[1]);
    char *sched_policy = argv[2];

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
            running = handle_request(&request);
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

    close(fd);
    unlink(CONTROLLER_FIFO);

    return 0;
}
