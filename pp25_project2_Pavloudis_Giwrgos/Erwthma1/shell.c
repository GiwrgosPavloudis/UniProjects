#include "shell.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

bool read_command(char *buffer, size_t size) {
    printf("my_shell$ ");
    fflush(stdout);

    if (fgets(buffer, (int)size, stdin) == NULL) {
        return false;
    }

    buffer[strcspn(buffer, "\n")] = '\0';

    if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "Exit") == 0) {
        return false;
    }

    return true;
}

int tokenize_command(char *buffer, char *argv[], int max_args) {
    int count = 0;
    char *token = strtok(buffer, " \t");

    while (token != NULL && count < max_args - 1) {
        argv[count++] = token;
        token = strtok(NULL, " \t");
    }

    argv[count] = NULL;
    return count;
}

int find_pipe(char *argv[]) {
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "|") == 0) {
            return i;
        }
    }
    return -1;
}

int parse_redirections(char *argv[], Redirection *redir) {
    int read_index = 0;
    int write_index = 0;

    redir->input_file = NULL;
    redir->output_file = NULL;
    redir->append_output = false;

    while (argv[read_index] != NULL) {
        if (strcmp(argv[read_index], "<") == 0) {
            if (argv[read_index + 1] == NULL) {
                fprintf(stderr, "Missing input filename after <\n");
                return -1;
            }
            redir->input_file = argv[read_index + 1];
            read_index += 2;
            continue;
        }

        if (strcmp(argv[read_index], ">") == 0 ||
            strcmp(argv[read_index], ">>") == 0) {
            if (argv[read_index + 1] == NULL) {
                fprintf(stderr, "Missing output filename after redirection.\n");
                return -1;
            }
            redir->append_output = (strcmp(argv[read_index], ">>") == 0);
            redir->output_file = argv[read_index + 1];
            read_index += 2;
            continue;
        }

        argv[write_index++] = argv[read_index++];
    }

    argv[write_index] = NULL;
    return write_index > 0 ? 0 : -1;
}

static int apply_redirections(const Redirection *redir) {
    if (redir->input_file != NULL) {
        int fd = open(redir->input_file, O_RDONLY);
        if (fd == -1) {
            perror("open input");
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2 input");
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (redir->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT |
                    (redir->append_output ? O_APPEND : O_TRUNC);
        int fd = open(redir->output_file, flags, 0644);
        if (fd == -1) {
            perror("open output");
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2 output");
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

static int wait_for_child(pid_t pid) {
    int status;

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "Process %d terminated by signal %d.\n",
                (int)pid, WTERMSIG(status));
    }

    return -1;
}

int execute_command(char *argv[], const Redirection *redir) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        if (apply_redirections(redir) == -1) {
            _exit(EXIT_FAILURE);
        }

        execvp(argv[0], argv);
        perror("execvp");
        _exit(127);
    }

    return wait_for_child(pid);
}

int execute_pipe(char *left[], const Redirection *left_redir,
                 char *right[], const Redirection *right_redir) {
    int pipefd[2];
    pid_t left_pid;
    pid_t right_pid;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }

    left_pid = fork();
    if (left_pid == -1) {
        perror("fork left");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (left_pid == 0) {
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2 pipe output");
            _exit(EXIT_FAILURE);
        }
        close(pipefd[0]);
        close(pipefd[1]);

        /* Explicit redirections override the pipe endpoint, as in a normal shell. */
        if (apply_redirections(left_redir) == -1) {
            _exit(EXIT_FAILURE);
        }

        execvp(left[0], left);
        perror("execvp left");
        _exit(127);
    }

    right_pid = fork();
    if (right_pid == -1) {
        perror("fork right");
        close(pipefd[0]);
        close(pipefd[1]);
        wait_for_child(left_pid);
        return -1;
    }

    if (right_pid == 0) {
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2 pipe input");
            _exit(EXIT_FAILURE);
        }
        close(pipefd[0]);
        close(pipefd[1]);

        if (apply_redirections(right_redir) == -1) {
            _exit(EXIT_FAILURE);
        }

        execvp(right[0], right);
        perror("execvp right");
        _exit(127);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    int left_status = wait_for_child(left_pid);
    int right_status = wait_for_child(right_pid);

    return (left_status == 0 && right_status == 0) ? 0 : -1;
}
