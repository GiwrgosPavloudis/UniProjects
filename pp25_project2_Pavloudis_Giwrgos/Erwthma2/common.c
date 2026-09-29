#include "exchange.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int send_line(int socket_fd, const char *text) {
    size_t len = strlen(text);
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(socket_fd, text + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }

    if (len == 0 || text[len - 1] != '\n') {
        const char newline = '\n';
        while (send(socket_fd, &newline, 1, 0) < 0) {
            if (errno != EINTR) {
                return -1;
            }
        }
    }

    return 0;
}

ssize_t recv_line(int socket_fd, char *buffer, size_t size) {
    size_t used = 0;

    if (size == 0) {
        return -1;
    }

    while (used + 1 < size) {
        char ch;
        ssize_t n = recv(socket_fd, &ch, 1, 0);

        if (n == 0) {
            break;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (ch == '\n') {
            break;
        }

        buffer[used++] = ch;
    }

    buffer[used] = '\0';
    return (ssize_t)used;
}
