#ifndef EXCHANGE_H
#define EXCHANGE_H

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <sys/types.h>

#define EXCHANGE_PORT 8081
#define EXCHANGE_BUFFER 512
#define USERNAME_LEN 50
#define PASSWORD_LEN 50
#define MAX_OWNERS 2

typedef struct {
    char username[USERNAME_LEN];
    char password[PASSWORD_LEN];
} User;

typedef struct {
    int account_id;
    char owners[MAX_OWNERS][USERNAME_LEN];
    int owner_count;
    double eur_balance;
    double usd_balance;
} Account;

int send_line(int socket_fd, const char *text);
ssize_t recv_line(int socket_fd, char *buffer, size_t size);

#endif
