#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "types.h"
#include "utils.h"

void strip_quotes(char *str) {
    if (strlen(str) < 2) return;

    if (str[0] == '\"' && str[strlen(str) - 1] == '\"') {
        memmove(str, str + 1, strlen(str) - 2);
        str[strlen(str) - 2] = '\0';
    }
}

char* get_next_token(char **str_ptr) {
    char *str = *str_ptr;

    // pula whitespace
    while (*str == ' ' || *str == '\n' || *str == '\t') str++;

    if (*str == '\0') {
        *str_ptr = str;
        return NULL;
    }

    char *start = str;
    bool inside_quote = false;

    while (*str != '\0') {
        if (*str == '\"') inside_quote = !inside_quote;
        else if ((*str == ' ' || *str == '\n' || *str == '\t') && !inside_quote) {
            *str = '\0';
            *str_ptr = str + 1;
            return start;
        }
        str++;
    }

    *str_ptr = str; // chegamos ao fim
    return start;
}

bool parse_command(char *str, Command *cmd) {
    memset(cmd, 0, sizeof(Command));
    int idx = 0;

    char *token = get_next_token(&str);
    if (!token) return false;

    while (token != NULL) {
        if (strcmp(token, "<") == 0) {
            cmd->in = get_next_token(&str);
            if (!cmd->in) {
                printerr("Syntax error: missing input file for '<'.\n");
                return false;
            }
            strip_quotes(cmd->in);
        } else if (strcmp(token, ">") == 0) {
            cmd->out = get_next_token(&str);
            cmd->append = false;
            if (!cmd->out) {
                printerr("Syntax error: missing output file for '>'.\n");
                return false;
            }
            strip_quotes(cmd->out);
        } else if (strcmp(token, ">>") == 0) {
            cmd->out = get_next_token(&str);
            cmd->append = true;
            if (!cmd->out) {
                printerr("Syntax error: missing output file for '>>'.\n");
                return false;
            }
            strip_quotes(cmd->out);
        } else if (strcmp(token, "2>") == 0) {
            cmd->err = get_next_token(&str);
            if (!cmd->out) {
                printerr("Syntax error: missing output file for '2>'.\n");
                return false;
            }
            strip_quotes(cmd->err);
        } else {
            if (idx > MAX_ARGS) {
                printerr("Syntax error: too many arguments.\n");
                return false;
            }
            strip_quotes(token);
            cmd->args[idx++] = token;
        }

        token = get_next_token(&str);
    }

    cmd->args[idx] = NULL;

    if (idx == 0) {
        printerr("Syntax error: redirection without a command.\n");
        return false;
    }

    return true;
}

bool parse_pipeline(char *str, Pipeline *pipeline) {
    pipeline->num_cmds = 0;
    bool inside_quote = false;
    char *start = str;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\"') inside_quote = !inside_quote;

        if (str[i] == '|' && !inside_quote) {
            str[i] = '\0';

            if (!parse_command(start, &pipeline->cmd[pipeline->num_cmds])) {
                printerr("Syntax error: invalid or empty command near pipe.\n");
                return false;
            }

            pipeline->num_cmds++;
            if (pipeline->num_cmds >= MAX_CMDS) {
                printerr("Syntax error: exceeded maximum number of commands.\n");
                return false;
            }

            start = &str[i + 1];
        }
    }

    if (inside_quote) {
        printerr("Syntax error: unmatched quotes.\n");
        return false;
    }

    if (!parse_command(start, &pipeline->cmd[pipeline->num_cmds])) {
        printerr("Syntax error: invalid command (are there trailing pipes?).\n");
        return false;
    }

    pipeline->num_cmds++;
    return true;
}
