#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stddef.h>

#define SHELL_BUFFER 2048
#define SHELL_MAX_ARGS 256

typedef struct {
    char *input_file;
    char *output_file;
    bool append_output;
} Redirection;

bool read_command(char *buffer, size_t size);
int tokenize_command(char *buffer, char *argv[], int max_args);
int find_pipe(char *argv[]);
int parse_redirections(char *argv[], Redirection *redir);
int execute_command(char *argv[], const Redirection *redir);
int execute_pipe(char *left[], const Redirection *left_redir,
                 char *right[], const Redirection *right_redir);

#endif
