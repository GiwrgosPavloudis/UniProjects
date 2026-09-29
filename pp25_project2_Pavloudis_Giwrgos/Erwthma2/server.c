#define _POSIX_C_SOURCE 200809L
#include "exchange.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define USERS_FILE "users.txt"
#define ACCOUNTS_FILE "accounts.txt"
#define EUR_TO_USD 1.10
#define USD_TO_EUR (1.0 / EUR_TO_USD)
#define TEXT_LINE 512

static void reap_children(int signal_number) {
    (void)signal_number;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
}

static int lock_file(int fd, short type) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = type;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    while (fcntl(fd, F_SETLKW, &lock) == -1) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return 0;
}

static int unlock_file(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    return fcntl(fd, F_SETLK, &lock);
}

static int ensure_data_files(void) {
    int fd = open(USERS_FILE, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        return -1;
    }
    close(fd);

    fd = open(ACCOUNTS_FILE, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        return -1;
    }
    close(fd);
    return 0;
}

/* Read one text line from a file descriptor. */
static ssize_t read_text_line(int fd, char *buffer, size_t size) {
    if (size == 0) {
        return -1;
    }

    size_t used = 0;
    while (used + 1 < size) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n == 0) {
            break;
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (c == '\n') {
            break;
        }
        buffer[used++] = c;
    }
    buffer[used] = '\0';
    return (ssize_t)used;
}

static int parse_user_line(const char *line, User *user) {
    return sscanf(line, "%49s %49s", user->username, user->password) == 2;
}

static int parse_account_line(const char *line, Account *account) {
    char owner2[USERNAME_LEN];
    int fields = sscanf(line, "%d %d %49s %49s %lf %lf",
                        &account->account_id,
                        &account->owner_count,
                        account->owners[0],
                        owner2,
                        &account->eur_balance,
                        &account->usd_balance);
    if (fields != 6) {
        return 0;
    }

    account->owners[1][0] = '\0';
    if (account->owner_count == 2 && strcmp(owner2, "-") != 0) {
        snprintf(account->owners[1], sizeof(account->owners[1]), "%s", owner2);
    }
    return account->owner_count == 1 || account->owner_count == 2;
}

static int write_account_line(int fd, const Account *account) {
    const char *owner2 = account->owner_count == 2 ? account->owners[1] : "-";
    char line[TEXT_LINE];
    int n = snprintf(line, sizeof(line), "%d %d %s %s %.2f %.2f\n",
                     account->account_id,
                     account->owner_count,
                     account->owners[0],
                     owner2,
                     account->eur_balance,
                     account->usd_balance);
    if (n < 0 || (size_t)n >= sizeof(line)) {
        return -1;
    }
    return write(fd, line, (size_t)n) == n ? 0 : -1;
}

static int load_all_accounts_locked(int fd, Account **accounts_out, size_t *count_out) {
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        return -1;
    }

    Account *accounts = NULL;
    size_t count = 0;
    char line[TEXT_LINE];

    for (;;) {
        ssize_t n = read_text_line(fd, line, sizeof(line));
        if (n < 0) {
            free(accounts);
            return -1;
        }
        if (n == 0) {
            char extra;
            ssize_t more = read(fd, &extra, 1);
            if (more == 0) {
                break;
            }
            if (more < 0) {
                free(accounts);
                return -1;
            }
            continue;
        }

        Account account;
        memset(&account, 0, sizeof(account));
        if (!parse_account_line(line, &account)) {
            continue;
        }

        Account *tmp = realloc(accounts, (count + 1) * sizeof(*accounts));
        if (tmp == NULL) {
            free(accounts);
            return -1;
        }
        accounts = tmp;
        accounts[count++] = account;
    }

    *accounts_out = accounts;
    *count_out = count;
    return 0;
}

static int save_all_accounts_locked(int fd, const Account *accounts, size_t count) {
    if (ftruncate(fd, 0) == -1 || lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        if (write_account_line(fd, &accounts[i]) == -1) {
            return -1;
        }
    }
    return 0;
}

static int user_exists(const char *username) {
    int fd = open(USERS_FILE, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    if (lock_file(fd, F_RDLCK) == -1) {
        close(fd);
        return 0;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        unlock_file(fd);
        close(fd);
        return 0;
    }

    char line[TEXT_LINE];
    User user;
    int found = 0;
    while (read_text_line(fd, line, sizeof(line)) > 0) {
        memset(&user, 0, sizeof(user));
        if (parse_user_line(line, &user) && strcmp(user.username, username) == 0) {
            found = 1;
            break;
        }
    }

    unlock_file(fd);
    close(fd);
    return found;
}

static int register_user(const char *username, const char *password,
                         char *response, size_t response_size) {
    int fd = open(USERS_FILE, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        snprintf(response, response_size, "Error opening users file.");
        return -1;
    }

    if (lock_file(fd, F_WRLCK) == -1) {
        close(fd);
        snprintf(response, response_size, "Error locking users file.");
        return -1;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        unlock_file(fd);
        close(fd);
        snprintf(response, response_size, "Could not read users file.");
        return -1;
    }

    char line[TEXT_LINE];
    User current;
    while (read_text_line(fd, line, sizeof(line)) > 0) {
        memset(&current, 0, sizeof(current));
        if (parse_user_line(line, &current) && strcmp(current.username, username) == 0) {
            unlock_file(fd);
            close(fd);
            snprintf(response, response_size, "Username already exists.");
            return 0;
        }
    }

    if (lseek(fd, 0, SEEK_END) == (off_t)-1) {
        unlock_file(fd);
        close(fd);
        snprintf(response, response_size, "Could not save user.");
        return -1;
    }

    int n = snprintf(line, sizeof(line), "%s %s\n", username, password);
    if (n < 0 || (size_t)n >= sizeof(line) ||
        write(fd, line, (size_t)n) != n) {
        unlock_file(fd);
        close(fd);
        snprintf(response, response_size, "Could not save user.");
        return -1;
    }

    unlock_file(fd);
    close(fd);
    snprintf(response, response_size, "Registration successful.");
    return 1;
}

static int login_user(const char *username, const char *password) {
    int fd = open(USERS_FILE, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    if (lock_file(fd, F_RDLCK) == -1) {
        close(fd);
        return 0;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        unlock_file(fd);
        close(fd);
        return 0;
    }

    char line[TEXT_LINE];
    User user;
    int valid = 0;
    while (read_text_line(fd, line, sizeof(line)) > 0) {
        memset(&user, 0, sizeof(user));
        if (parse_user_line(line, &user) &&
            strcmp(user.username, username) == 0 &&
            strcmp(user.password, password) == 0) {
            valid = 1;
            break;
        }
    }

    unlock_file(fd);
    close(fd);
    return valid;
}

static int account_owned_by(const Account *account, const char *username) {
    for (int i = 0; i < account->owner_count; i++) {
        if (strcmp(account->owners[i], username) == 0) {
            return 1;
        }
    }
    return 0;
}

static int create_account(const char *username, const char *second_owner,
                          char *response, size_t response_size) {
    if (second_owner != NULL) {
        if (strcmp(username, second_owner) == 0) {
            snprintf(response, response_size,
                     "Joint account needs a different second owner.");
            return 0;
        }
        if (!user_exists(second_owner)) {
            snprintf(response, response_size,
                     "Second owner is not a registered user.");
            return 0;
        }
    }

    int fd = open(ACCOUNTS_FILE, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        snprintf(response, response_size, "Error opening accounts file.");
        return -1;
    }

    if (lock_file(fd, F_WRLCK) == -1) {
        close(fd);
        snprintf(response, response_size, "Error locking accounts file.");
        return -1;
    }

    Account *accounts = NULL;
    size_t count = 0;
    if (load_all_accounts_locked(fd, &accounts, &count) == -1) {
        unlock_file(fd);
        close(fd);
        snprintf(response, response_size, "Could not read accounts file.");
        return -1;
    }

    int next_id = 1;
    for (size_t i = 0; i < count; i++) {
        if (accounts[i].account_id >= next_id) {
            next_id = accounts[i].account_id + 1;
        }
    }

    Account *tmp = realloc(accounts, (count + 1) * sizeof(*accounts));
    if (tmp == NULL) {
        free(accounts);
        unlock_file(fd);
        close(fd);
        snprintf(response, response_size, "Could not save account.");
        return -1;
    }
    accounts = tmp;

    Account *account = &accounts[count];
    memset(account, 0, sizeof(*account));
    account->account_id = next_id;
    account->owner_count = second_owner == NULL ? 1 : 2;
    snprintf(account->owners[0], sizeof(account->owners[0]), "%s", username);
    if (second_owner != NULL) {
        snprintf(account->owners[1], sizeof(account->owners[1]), "%s", second_owner);
    }
    count++;

    int saved = save_all_accounts_locked(fd, accounts, count);
    free(accounts);
    unlock_file(fd);
    close(fd);

    if (saved == -1) {
        snprintf(response, response_size, "Could not save account.");
        return -1;
    }

    if (second_owner == NULL) {
        snprintf(response, response_size,
                 "Individual account created. Account ID: %d", next_id);
    } else {
        snprintf(response, response_size,
                 "Joint account created for %s and %s. Account ID: %d",
                 username, second_owner, next_id);
    }
    return 1;
}

static int find_account(Account *accounts, size_t count, int account_id) {
    for (size_t i = 0; i < count; i++) {
        if (accounts[i].account_id == account_id) {
            return (int)i;
        }
    }
    return -1;
}

static void deposit(const char *username, int account_id, const char *currency,
                    double amount, char *response, size_t response_size) {
    if (amount <= 0.0) {
        snprintf(response, response_size, "Amount must be positive.");
        return;
    }

    int fd = open(ACCOUNTS_FILE, O_RDWR);
    if (fd == -1 || lock_file(fd, F_WRLCK) == -1) {
        if (fd != -1) close(fd);
        snprintf(response, response_size, "Could not access accounts file.");
        return;
    }

    Account *accounts = NULL;
    size_t count = 0;
    if (load_all_accounts_locked(fd, &accounts, &count) == -1) {
        snprintf(response, response_size, "Could not read accounts file.");
    } else {
        int index = find_account(accounts, count, account_id);
        if (index < 0) {
            snprintf(response, response_size, "Account not found.");
        } else if (!account_owned_by(&accounts[index], username)) {
            snprintf(response, response_size, "You are not an owner of this account.");
        } else if (strcmp(currency, "EUR") == 0) {
            accounts[index].eur_balance += amount;
            if (save_all_accounts_locked(fd, accounts, count) == -1) {
                snprintf(response, response_size, "Could not save account.");
            } else {
                snprintf(response, response_size, "Deposit OK. EUR balance: %.2f",
                         accounts[index].eur_balance);
            }
        } else if (strcmp(currency, "USD") == 0) {
            accounts[index].usd_balance += amount;
            if (save_all_accounts_locked(fd, accounts, count) == -1) {
                snprintf(response, response_size, "Could not save account.");
            } else {
                snprintf(response, response_size, "Deposit OK. USD balance: %.2f",
                         accounts[index].usd_balance);
            }
        } else {
            snprintf(response, response_size, "Currency must be EUR or USD.");
        }
    }

    free(accounts);
    unlock_file(fd);
    close(fd);
}

static void withdraw_money(const char *username, int account_id, const char *currency,
                           double amount, char *response, size_t response_size) {
    if (amount <= 0.0) {
        snprintf(response, response_size, "Amount must be positive.");
        return;
    }

    int fd = open(ACCOUNTS_FILE, O_RDWR);
    if (fd == -1 || lock_file(fd, F_WRLCK) == -1) {
        if (fd != -1) close(fd);
        snprintf(response, response_size, "Could not access accounts file.");
        return;
    }

    Account *accounts = NULL;
    size_t count = 0;
    if (load_all_accounts_locked(fd, &accounts, &count) == -1) {
        snprintf(response, response_size, "Could not read accounts file.");
    } else {
        int index = find_account(accounts, count, account_id);
        if (index < 0) {
            snprintf(response, response_size, "Account not found.");
        } else if (!account_owned_by(&accounts[index], username)) {
            snprintf(response, response_size, "You are not an owner of this account.");
        } else if (strcmp(currency, "EUR") == 0) {
            if (accounts[index].eur_balance < amount) {
                snprintf(response, response_size, "Insufficient EUR balance.");
            } else {
                accounts[index].eur_balance -= amount;
                if (save_all_accounts_locked(fd, accounts, count) == -1) {
                    snprintf(response, response_size, "Could not save account.");
                } else {
                    snprintf(response, response_size, "Withdrawal OK. EUR balance: %.2f",
                             accounts[index].eur_balance);
                }
            }
        } else if (strcmp(currency, "USD") == 0) {
            if (accounts[index].usd_balance < amount) {
                snprintf(response, response_size, "Insufficient USD balance.");
            } else {
                accounts[index].usd_balance -= amount;
                if (save_all_accounts_locked(fd, accounts, count) == -1) {
                    snprintf(response, response_size, "Could not save account.");
                } else {
                    snprintf(response, response_size, "Withdrawal OK. USD balance: %.2f",
                             accounts[index].usd_balance);
                }
            }
        } else {
            snprintf(response, response_size, "Currency must be EUR or USD.");
        }
    }

    free(accounts);
    unlock_file(fd);
    close(fd);
}

static void exchange_money(const char *username, int account_id, int type,
                           double amount, char *response, size_t response_size) {
    if (amount <= 0.0) {
        snprintf(response, response_size, "Amount must be positive.");
        return;
    }

    int fd = open(ACCOUNTS_FILE, O_RDWR);
    if (fd == -1 || lock_file(fd, F_WRLCK) == -1) {
        if (fd != -1) close(fd);
        snprintf(response, response_size, "Could not access accounts file.");
        return;
    }

    Account *accounts = NULL;
    size_t count = 0;
    if (load_all_accounts_locked(fd, &accounts, &count) == -1) {
        snprintf(response, response_size, "Could not read accounts file.");
    } else {
        int index = find_account(accounts, count, account_id);
        if (index < 0) {
            snprintf(response, response_size, "Account not found.");
        } else if (!account_owned_by(&accounts[index], username)) {
            snprintf(response, response_size, "You are not an owner of this account.");
        } else if (type == 1) {
            if (accounts[index].eur_balance < amount) {
                snprintf(response, response_size, "Insufficient EUR balance.");
            } else {
                double received = amount * EUR_TO_USD;
                accounts[index].eur_balance -= amount;
                accounts[index].usd_balance += received;
                if (save_all_accounts_locked(fd, accounts, count) == -1) {
                    snprintf(response, response_size, "Could not save account.");
                } else {
                    snprintf(response, response_size,
                             "Exchange OK: %.2f EUR -> %.2f USD. Balances EUR %.2f / USD %.2f",
                             amount, received,
                             accounts[index].eur_balance, accounts[index].usd_balance);
                }
            }
        } else if (type == 2) {
            if (accounts[index].usd_balance < amount) {
                snprintf(response, response_size, "Insufficient USD balance.");
            } else {
                double received = amount * USD_TO_EUR;
                accounts[index].usd_balance -= amount;
                accounts[index].eur_balance += received;
                if (save_all_accounts_locked(fd, accounts, count) == -1) {
                    snprintf(response, response_size, "Could not save account.");
                } else {
                    snprintf(response, response_size,
                             "Exchange OK: %.2f USD -> %.2f EUR. Balances EUR %.2f / USD %.2f",
                             amount, received,
                             accounts[index].eur_balance, accounts[index].usd_balance);
                }
            }
        } else {
            snprintf(response, response_size, "Exchange type must be 1 or 2.");
        }
    }

    free(accounts);
    unlock_file(fd);
    close(fd);
}

static int split_tokens(char *line, char *argv[], int max_args) {
    int argc = 0;
    char *token = strtok(line, " \t");
    while (token != NULL && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    return argc;
}

static void handle_client(int client_socket) {
    char line[EXCHANGE_BUFFER];
    char response[EXCHANGE_BUFFER];
    char logged_user[USERNAME_LEN] = "";

    for (;;) {
        ssize_t n = recv_line(client_socket, line, sizeof(line));
        if (n <= 0) {
            return;
        }

        char *argv[8];
        int argc = split_tokens(line, argv, 8);
        response[0] = '\0';

        if (argc == 0) {
            snprintf(response, sizeof(response), "Invalid command.");
        } else if (strcmp(argv[0], "REGISTER") == 0) {
            if (argc != 3) {
                snprintf(response, sizeof(response), "Usage: REGISTER username password");
            } else {
                register_user(argv[1], argv[2], response, sizeof(response));
            }
        } else if (strcmp(argv[0], "LOGIN") == 0) {
            if (argc != 3) {
                snprintf(response, sizeof(response), "Usage: LOGIN username password");
            } else if (login_user(argv[1], argv[2])) {
                snprintf(logged_user, sizeof(logged_user), "%s", argv[1]);
                snprintf(response, sizeof(response), "Login successful.");
            } else {
                snprintf(response, sizeof(response), "Invalid username or password.");
            }
        } else if (strcmp(argv[0], "EXIT") == 0) {
            send_line(client_socket, "Goodbye.");
            return;
        } else if (logged_user[0] == '\0') {
            snprintf(response, sizeof(response), "You must login first.");
        } else if (strcmp(argv[0], "CREATE_ACCOUNT") == 0) {
            if (argc == 2 && strcmp(argv[1], "INDIVIDUAL") == 0) {
                create_account(logged_user, NULL, response, sizeof(response));
            } else if (argc == 3 && strcmp(argv[1], "JOINT") == 0) {
                create_account(logged_user, argv[2], response, sizeof(response));
            } else {
                snprintf(response, sizeof(response),
                         "Usage: CREATE_ACCOUNT INDIVIDUAL or CREATE_ACCOUNT JOINT username");
            }
        } else if (strcmp(argv[0], "DEPOSIT") == 0 && argc == 4) {
            deposit(logged_user, atoi(argv[1]), argv[2], atof(argv[3]),
                    response, sizeof(response));
        } else if (strcmp(argv[0], "WITHDRAW") == 0 && argc == 4) {
            withdraw_money(logged_user, atoi(argv[1]), argv[2], atof(argv[3]),
                           response, sizeof(response));
        } else if (strcmp(argv[0], "EXCHANGE") == 0 && argc == 4) {
            exchange_money(logged_user, atoi(argv[1]), atoi(argv[2]), atof(argv[3]),
                           response, sizeof(response));
        } else {
            snprintf(response, sizeof(response), "Unknown or malformed command.");
        }

        if (send_line(client_socket, response) == -1) {
            return;
        }
    }
}

int main(void) {
    if (ensure_data_files() == -1) {
        perror("data files");
        return EXIT_FAILURE;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = reap_children;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &action, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int yes = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(EXCHANGE_PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server_socket);
        return EXIT_FAILURE;
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Exchange server listening on port %d.\n", EXCHANGE_PORT);

    for (;;) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(client_socket);
            continue;
        }

        if (pid == 0) {
            close(server_socket);
            handle_client(client_socket);
            close(client_socket);
            _exit(EXIT_SUCCESS);
        }

        close(client_socket);
    }
}
