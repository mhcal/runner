#include "types.h"
#include "utils.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>

int parse(int argc, char *argv[], Request *request) {
    if (argc < 2) return -1;
    if (argv[1][0] != '-') return -1;

    request->runner_pid = getpid();
    memset(request->cmd, 0, CMD_LEN);

    switch(argv[1][1]) {
        case 'e':
            if (argc < 4) return -1;

            request->op = EXECUTE;
            request->user_id = atoi(argv[2]);

            for (int i = 3; i < argc; i++) {
                strcat(request->cmd, argv[i]);
                if (i < argc - 1) strcat(request->cmd, " ");
            }

            return 0;
        case 'c':
            request->op = CONSULT;
            return 0;
        case 's':
            request->op = SHUTDOWN;
            return 0;
    }

    return -1;
}

void send_request(const Request *request, Response *response) {
    char runner_fifo[CMD_LEN];
    char msg[CMD_LEN];

    snprintf(runner_fifo, sizeof(runner_fifo), RUNNER_FIFO "_%d", request->runner_pid);

    if (mkfifo(runner_fifo, 0666) == -1)
        perror("[runner] failed to create runner FIFO");

    int fd_controller = open(CONTROLLER_FIFO, O_WRONLY);
    if (fd_controller == -1) {
        printerr("[runner] error: no controller found (is it running?)\n");
        unlink(runner_fifo);
        exit(1);
    }

    write(fd_controller, request, sizeof(Request));
    close(fd_controller);

    snprintf(msg, sizeof(msg), "[runner] command %d submitted\n", request->runner_pid);
    write(STDOUT_FILENO, msg, strlen(msg));

    int fd_runner = open(runner_fifo, O_RDONLY);
    if (fd_runner != -1) {
        read(fd_runner, response, sizeof(Response));
        close(fd_runner);
    }

    else
        perror("[runner] failed to open runner FIFO");

    unlink(runner_fifo);
}

void handle_response(int argc, char *argv[], const Request *request, const Response *response) {
    char msg[256];

    if (!response->allowed) {
        printerr("[runner] error: controller has denied the request\n");
        return;
    }

    if (request->op == EXECUTE) {
        snprintf(msg, sizeof(msg), "[runner] executing command %d...\n", request->runner_pid);
        write(STDOUT_FILENO, msg, strlen(msg));

        pid_t pid = fork();

        if (pid < 0) {
            printerr("fork failed\n");
            return;
        }

        if (pid == 0) {
            // child
            char *exec_args[argc - 2];
            for (int i = 3; i < argc; i++) {
                exec_args[i - 3] = argv[i];
            }
            exec_args[argc - 3] = NULL;

            execvp(exec_args[0], exec_args);

            // fallback
            printerr("Error executing command\n");
            exit(1);
        }

        // parent
        int status;
        waitpid(pid, &status, 0);
        snprintf(msg, sizeof(msg), "[runner] command %d finished\n", request->runner_pid);
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

int main(int argc, char *argv[]) {
    Request request;
    if (parse(argc, argv, &request) == -1) {
        printerr("Error. Usage: ./runner -e [user-id] [command] [args] | -c | -s\n");
        return 1;
    }

    Response response;
    send_request(&request, &response);
    handle_response(argc, argv, &request, &response);

    return 0;
}
