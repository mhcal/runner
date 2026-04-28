#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "types.h"

void strip_quotes(char *str);
char* get_next_token(char **str_ptr);
bool parse_command(char *str, Command *cmd);
bool parse_pipeline(char *str, Pipeline *pipeline);

#endif
