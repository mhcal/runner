#include "types.h"
#include "utils.h"
#include "parse.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>

void notify_finished(const Request *request) {
    Request finished;
    memset(&finished, 0, sizeof(Request));
    finished.op = FINISHED;
    finished.user_id = request->user_id;
    finished.runner_pid = request->runner_pid;

    int fd = open(CONTROLLER_FIFO, O_WRONLY);
    if (fd != -1) {
        write(fd, &finished, sizeof(Request));
        close(fd);
    } else {
        perror("[runner] failed to notify the controller of finished process\n");
    }
}

void send_request(const Request *request, Response *response) {
    char runner_fifo[256];
    char msg[256];

    snprintf(runner_fifo, sizeof(runner_fifo), RUNNER_FIFO "_%d", request->runner_pid);

    if (mkfifo(runner_fifo, 0666) == -1)
        perror("[runner] failed to create runner FIFO");

    int fd_runner = open(runner_fifo, O_RDWR);
    int fd_controller = open(CONTROLLER_FIFO, O_WRONLY);

    if (fd_controller == -1) {
        printerr("[runner] error: no controller found (is it running?)\n");
        unlink(runner_fifo);
        exit(1);
    }

    write(fd_controller, request, sizeof(Request));
    close(fd_controller);

    if (request->op == EXECUTE) {
        snprintf(msg, sizeof(msg), "[runner] command %d submitted\n", request->runner_pid);
        write(STDOUT_FILENO, msg, strlen(msg));
    } else if (request->op == SHUTDOWN) {
        snprintf(msg, sizeof(msg), "[runner] sent shutdown notification\n");
        write(STDOUT_FILENO, msg, strlen(msg));
    }

    if (fd_runner != -1) {
        read(fd_runner, response, sizeof(Response));
        close(fd_runner);
    } else
        perror("[runner] failed to open runner FIFO");

    unlink(runner_fifo);
}

void execute_pipeline(Pipeline *pipeline) {
    int prev_fd = -1;
    int fd[2];
    pid_t pid[MAX_CMDS];

    for (int i = 0; i < pipeline->num_cmds; i++) {
        Command *cmd = &pipeline->cmd[i];

        if (i < pipeline->num_cmds - 1) {
            if (pipe(fd) < 0) {
                perror("pipe failed");
                return;
            }
        }

        pid[i] = fork();
        if (pid[i] < 0) {
            perror("fork failed");
            return;
        }

        if (pid[i] == 0) {
            // child
            if (prev_fd != -1) {
                // não é o primeiro comando
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            if (i < pipeline->num_cmds - 1) {
                // não é o último comando
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]); // filho não lê do seu próprio pipe
                close(fd[1]);
            }

            if (cmd->in) {
                int fd_in = open(cmd->in, O_RDONLY);
                if (fd_in < 0) {
                    perror("opening input file failed");
                    return;
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            if (cmd->out) {
                int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
                int fd_out = open(cmd->out, flags, 0644);
                if (fd_out < 0) {
                    perror("opening output file failed");
                    return;
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            if (cmd->err) {
                int flags = O_WRONLY | O_CREAT | O_TRUNC;
                int fd_err = open(cmd->err, flags, 0644);
                if (fd_err < 0) {
                    perror("opening error file failed");
                    return;
                }
                dup2(fd_err, STDERR_FILENO);
                close(fd_err);
            }

            // finalmente executamos o comando
            execvp(cmd->args[0], cmd->args);

            // fallback
            printerr("Error executing command\n");
            exit(1);
        } else {
            // parent
            if (prev_fd != -1) close(prev_fd); // pai já não precisa da pipe anterior

            // p/ próxima iteração, a pipe anterior é a leitura da pipe atual
            if (i < pipeline->num_cmds - 1) {
                close(fd[1]); // pai não escreve para a pipe
                prev_fd = fd[0];
            }
        }
    }

    // pai espera pelo fim de cada filho antes de terminar
    for (int i = 0; i < pipeline->num_cmds; i++) {
        int status;
        waitpid(pid[i], &status, 0);
    }
}

void handle_response(const Request *request, const Response *response, Pipeline *pipeline) {
    if (!response->allowed) {
        printerr("[runner] error: controller has denied the request - ");
        printerr(response->status);
        return;
    }

    if (request->op == EXECUTE) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[runner] executing command %d...\n", request->runner_pid);
        write(STDOUT_FILENO, msg, strlen(msg));

        execute_pipeline(pipeline);

        snprintf(msg, sizeof(msg), "[runner] command %d finished\n", request->runner_pid);
        write(STDOUT_FILENO, msg, strlen(msg));

        notify_finished(request);
    }

    else if (request->op == CONSULT) {
        write(STDOUT_FILENO, response->status, strlen(response->status));
    }
}

void usage_error() {
    printerr("Error. Usage: ./runner -e [user-id] [command] [args] | -c | -s\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argv[1][0] != '-') usage_error();

    Request request;
    request.runner_pid = getpid();
    Pipeline pipeline;

    switch (argv[1][1]) {
        case 'e':
            if (argc < 4 || !parse_pipeline(argv[3], &pipeline)) usage_error();
            request.op = EXECUTE;
            request.user_id = atoi(argv[2]);
            break;
        case 'c':
            if (argc != 2) usage_error();
            request.op = CONSULT;
            break;
        case 's':
            if (argc != 2) usage_error();
            request.op = SHUTDOWN;
            break;
        default:
            usage_error();
    }

    Response response;
    send_request(&request, &response);
    handle_response(&request, &response, &pipeline);

    return 0;
}
