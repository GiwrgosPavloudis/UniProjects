#include "auction.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

static int parse_positive_int(const char *text, int *value) {
    char *end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed <= 0 || parsed > 1000) {
        return -1;
    }
    *value = (int)parsed;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_bidders>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int bidder_count;
    if (parse_positive_int(argv[1], &bidder_count) == -1) {
        fprintf(stderr, "number_of_bidders must be an integer from 1 to 1000.\n");
        return EXIT_FAILURE;
    }

    key_t key = get_auction_key();
    if (key == (key_t)-1) {
        perror("ftok");
        return EXIT_FAILURE;
    }

    int shmid = shmget(key, sizeof(auction_data_t), 0666);
    if (shmid == -1) {
        perror("shmget (run ./auction_init first)");
        return EXIT_FAILURE;
    }

    auction_data_t *data = shmat(shmid, NULL, 0);
    if (data == (void *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    int semid = semget(key, SEM_COUNT, 0666);
    if (semid == -1) {
        perror("semget (run ./auction_init first)");
        shmdt(data);
        return EXIT_FAILURE;
    }

    for (int round = 1; round <= AUCTION_ROUNDS; ++round) {
        if (lock_memory(semid) == -1) {
            perror("semop lock");
            shmdt(data);
            return EXIT_FAILURE;
        }

        data->current_round = round;
        data->highest_bid = 0;
        data->highest_bidder_id = -1;
        data->round_open = 1;

        if (unlock_memory(semid) == -1) {
            perror("semop unlock");
            shmdt(data);
            return EXIT_FAILURE;
        }

        printf("\n=== Starting round %d/%d (%d seconds) ===\n",
               round, AUCTION_ROUNDS, ROUND_DURATION);
        fflush(stdout);

        if (signal_round(semid, bidder_count) == -1) {
            perror("semop start round");
            shmdt(data);
            return EXIT_FAILURE;
        }

        sleep(ROUND_DURATION);

        if (lock_memory(semid) == -1) {
            perror("semop lock");
            shmdt(data);
            return EXIT_FAILURE;
        }

        data->round_open = 0;
        int highest_bid = data->highest_bid;
        int winner = data->highest_bidder_id;

        if (unlock_memory(semid) == -1) {
            perror("semop unlock");
            shmdt(data);
            return EXIT_FAILURE;
        }

        if (winner == -1) {
            printf("Round %d ended with no valid bids.\n", round);
        } else {
            printf("Round %d ended. Highest bid: %d by bidder %d.\n",
                   round, highest_bid, winner);
        }
        fflush(stdout);
    }

    if (lock_memory(semid) == -1) {
        perror("semop lock");
        shmdt(data);
        return EXIT_FAILURE;
    }
    data->round_open = 0;
    data->auction_active = 0;
    if (unlock_memory(semid) == -1) {
        perror("semop unlock");
        shmdt(data);
        return EXIT_FAILURE;
    }

    if (signal_round(semid, bidder_count) == -1) {
        perror("semop finish auction");
        shmdt(data);
        return EXIT_FAILURE;
    }

    /* Give awakened bidders time to observe auction_active == 0 and detach. */
    for (int attempt = 0; attempt < 5; ++attempt) {
        sleep(1);
        if (lock_memory(semid) == -1) {
            perror("semop lock");
            shmdt(data);
            return EXIT_FAILURE;
        }
        int active = data->active_bidders;
        if (unlock_memory(semid) == -1) {
            perror("semop unlock");
            shmdt(data);
            return EXIT_FAILURE;
        }
        if (active == 0) {
            break;
        }
    }

    if (lock_memory(semid) == -1) {
        perror("semop lock");
        shmdt(data);
        return EXIT_FAILURE;
    }

    int total = data->bid_count;
    if (total > MAX_BIDS) {
        total = MAX_BIDS;
    }

    printf("\n=== Complete auction history ===\n");
    for (int i = 0; i < total; ++i) {
        const bid_record_t *record = &data->history[i];
        printf("%3d. Round %d - Bidder %d - Bid %d%s\n",
               i + 1,
               record->round,
               record->bidder_id,
               record->amount,
               record->accepted ? " (new highest)" : " (too low)");
    }
    if (total == 0) {
        printf("No bids were recorded.\n");
    }

    if (unlock_memory(semid) == -1) {
        perror("semop unlock");
        shmdt(data);
        return EXIT_FAILURE;
    }

    if (shmdt(data) == -1) {
        perror("shmdt");
        return EXIT_FAILURE;
    }

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID");
        return EXIT_FAILURE;
    }
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
        return EXIT_FAILURE;
    }

    printf("Auction finished. IPC resources removed.\n");
    return EXIT_SUCCESS;
}
