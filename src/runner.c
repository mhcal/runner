#include "types.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
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

// test function
void send_request(const Request *request, Response *response) {
    char msg[256];

    memset(response, 0, sizeof(Response));

    snprintf(msg, sizeof(msg), "[runner] command %d submitted\n", request->runner_pid);

    switch(request->op) {
        // for now, we will only allow execute requests
        case EXECUTE:
            response->allowed = 1;
            break;
        default:
            response->allowed = 0;
            break;
    }
}

void handle_response(int argc, char *argv[], const Request *request, const Response *response) {
    char msg[256];
    
    if (!response->allowed) {
        char err[] = "[runner] error: controller has denied the request";
        write(STDERR_FILENO, err, strlen(err));
        return;
    }

    if (request->op == EXECUTE) {
        snprintf(msg, sizeof(msg), "[runner] executing command %d...\n", request->runner_pid);
        write(STDOUT_FILENO, msg, strlen(msg));

        pid_t pid = fork();

        if (pid < 0) {
            char err[] = "fork failed\n";
            write(STDERR_FILENO, err, strlen(err));
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
            char err[] = "Error executing command\n";
            write(STDERR_FILENO, err, strlen(err));
            _exit(1); 
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
        char err[] = "Error. Usage: ./runner -e [user-id] [command] [args] | -c | -s\n";
        write(STDERR_FILENO, err, strlen(err));
        return 1;
    }

    Response response;
    send_request(&request, &response);
    handle_response(argc, argv, &request, &response);

    return 0;
}
