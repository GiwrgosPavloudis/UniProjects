#include "shell.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    char command[SHELL_BUFFER];
    char *arguments[SHELL_MAX_ARGS];

    while (read_command(command, sizeof(command))) {
        int argc = tokenize_command(command, arguments, SHELL_MAX_ARGS);

        if (argc == 0) {
            continue;
        }

        int pipe_pos = find_pipe(arguments);

        if (pipe_pos == -1) {
            Redirection redir;
            if (parse_redirections(arguments, &redir) == 0) {
                execute_command(arguments, &redir);
            }
            continue;
        }

        /* Required part: one pipe. Reject a second pipe instead of misparsing it. */
        for (int i = pipe_pos + 1; arguments[i] != NULL; i++) {
            if (strcmp(arguments[i], "|") == 0) {
                fprintf(stderr,
                        "This version supports one pipe (multiple pipes are the bonus).\n");
                pipe_pos = -2;
                break;
            }
        }
        if (pipe_pos == -2) {
            continue;
        }

        arguments[pipe_pos] = NULL;
        char **left = arguments;
        char **right = &arguments[pipe_pos + 1];

        if (left[0] == NULL || right[0] == NULL) {
            fprintf(stderr, "Invalid pipe command.\n");
            continue;
        }

        Redirection left_redir;
        Redirection right_redir;

        if (parse_redirections(left, &left_redir) == -1 ||
            parse_redirections(right, &right_redir) == -1) {
            continue;
        }

        execute_pipe(left, &left_redir, right, &right_redir);
    }

    printf("Shell terminated.\n");
    return 0;
}
