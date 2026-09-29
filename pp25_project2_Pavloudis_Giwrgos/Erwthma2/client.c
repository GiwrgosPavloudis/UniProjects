#include "exchange.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void read_text(const char *prompt, char *buffer, size_t size) {
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }
    buffer[strcspn(buffer, "\n")] = '\0';
}

static int read_choice(void) {
    char line[32];
    char *end = NULL;
    long value;

    read_text("Choice: ", line, sizeof(line));
    value = strtol(line, &end, 10);
    if (end == line || *end != '\0') {
        return -1;
    }
    return (int)value;
}

static void build_query(int choice, char *query, size_t size) {
    char username[USERNAME_LEN];
    char password[PASSWORD_LEN];
    char other_user[USERNAME_LEN];
    char account[32];
    char currency[16];
    char amount[64];
    char type[16];

    query[0] = '\0';

    switch (choice) {
        case 1:
            read_text("Username: ", username, sizeof(username));
            read_text("Password: ", password, sizeof(password));
            snprintf(query, size, "REGISTER %s %s", username, password);
            break;
        case 2:
            read_text("Username: ", username, sizeof(username));
            read_text("Password: ", password, sizeof(password));
            snprintf(query, size, "LOGIN %s %s", username, password);
            break;
        case 3:
            read_text("Account type (1=individual, 2=joint): ", type, sizeof(type));
            if (strcmp(type, "2") == 0) {
                read_text("Second owner's username: ", other_user, sizeof(other_user));
                snprintf(query, size, "CREATE_ACCOUNT JOINT %s", other_user);
            } else {
                snprintf(query, size, "CREATE_ACCOUNT INDIVIDUAL");
            }
            break;
        case 4:
            read_text("Account ID: ", account, sizeof(account));
            read_text("Currency (EUR/USD): ", currency, sizeof(currency));
            read_text("Amount: ", amount, sizeof(amount));
            snprintf(query, size, "DEPOSIT %s %s %s", account, currency, amount);
            break;
        case 5:
            read_text("Account ID: ", account, sizeof(account));
            read_text("Currency (EUR/USD): ", currency, sizeof(currency));
            read_text("Amount: ", amount, sizeof(amount));
            snprintf(query, size, "WITHDRAW %s %s %s", account, currency, amount);
            break;
        case 6:
            read_text("Account ID: ", account, sizeof(account));
            read_text("Exchange (1=EUR->USD, 2=USD->EUR): ", type, sizeof(type));
            read_text("Amount to exchange: ", amount, sizeof(amount));
            snprintf(query, size, "EXCHANGE %s %s %s", account, type, amount);
            break;
        case 7:
            snprintf(query, size, "EXIT");
            break;
        default:
            break;
    }
}

static void run_client(int socket_fd) {
    char query[EXCHANGE_BUFFER];
    char response[EXCHANGE_BUFFER];

    for (;;) {
        printf("\n1. Register\n");
        printf("2. Login\n");
        printf("3. Create account\n");
        printf("4. Deposit money\n");
        printf("5. Withdraw money\n");
        printf("6. Exchange currency\n");
        printf("7. Exit\n");

        int choice = read_choice();
        if (choice < 1 || choice > 7) {
            printf("Please choose 1-7.\n");
            continue;
        }

        build_query(choice, query, sizeof(query));
        if (send_line(socket_fd, query) == -1) {
            perror("send");
            return;
        }

        ssize_t n = recv_line(socket_fd, response, sizeof(response));
        if (n < 0) {
            perror("recv");
            return;
        }
        if (n == 0) {
            printf("Server closed the connection.\n");
            return;
        }

        printf("Server: %s\n", response);

        if (choice == 7) {
            return;
        }
    }
}

int main(void) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(EXCHANGE_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid server address.\n");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    if (connect(socket_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1) {
        perror("connect");
        close(socket_fd);
        return EXIT_FAILURE;
    }

    printf("Connected to exchange server on port %d.\n", EXCHANGE_PORT);
    run_client(socket_fd);
    close(socket_fd);
    return 0;
}
