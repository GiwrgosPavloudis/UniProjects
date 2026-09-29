#include "auction.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

static int parse_bidder_id(const char *text, int *value) {
    char *end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed <= 0 || parsed > 1000000) {
        return -1;
    }
    *value = (int)parsed;
    return 0;
}

static void unregister_bidder(auction_data_t *data, int semid) {
    if (lock_memory(semid) == 0) {
        if (data->active_bidders > 0) {
            data->active_bidders--;
        }
        (void)unlock_memory(semid);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <bidder_id>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int bidder_id;
    if (parse_bidder_id(argv[1], &bidder_id) == -1) {
        fprintf(stderr, "bidder_id must be a positive integer.\n");
        return EXIT_FAILURE;
    }

    srand((unsigned int)(time(NULL) ^ (unsigned int)getpid() ^ (unsigned int)bidder_id));

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

    if (lock_memory(semid) == -1) {
        perror("semop lock");
        shmdt(data);
        return EXIT_FAILURE;
    }
    data->active_bidders++;
    if (unlock_memory(semid) == -1) {
        perror("semop unlock");
        shmdt(data);
        return EXIT_FAILURE;
    }

    printf("Bidder %d is ready and waiting for rounds.\n", bidder_id);
    fflush(stdout);

    int last_round = 0;

    for (;;) {
        if (wait_for_round(semid) == -1) {
            perror("semop wait round");
            break;
        }

        if (lock_memory(semid) == -1) {
            perror("semop lock");
            break;
        }

        if (!data->auction_active) {
            if (data->active_bidders > 0) {
                data->active_bidders--;
            }
            (void)unlock_memory(semid);
            printf("Auction ended. Bidder %d exits.\n", bidder_id);
            shmdt(data);
            return EXIT_SUCCESS;
        }

        int round = data->current_round;
        int round_open = data->round_open;

        if (unlock_memory(semid) == -1) {
            perror("semop unlock");
            break;
        }

        if (!round_open || round <= last_round) {
            continue;
        }

        int delay = rand() % 3 + 1;
        sleep((unsigned int)delay);
        int new_bid = rand() % 100 + 1;

        if (lock_memory(semid) == -1) {
            perror("semop lock");
            break;
        }

        if (!data->auction_active) {
            if (data->active_bidders > 0) {
                data->active_bidders--;
            }
            (void)unlock_memory(semid);
            printf("Auction ended. Bidder %d exits.\n", bidder_id);
            shmdt(data);
            return EXIT_SUCCESS;
        }

        if (!data->round_open || data->current_round != round) {
            if (unlock_memory(semid) == -1) {
                perror("semop unlock");
                break;
            }
            last_round = round;
            printf("Bidder %d missed the time window for round %d.\n", bidder_id, round);
            fflush(stdout);
            continue;
        }

        int accepted = new_bid > data->highest_bid;
        if (accepted) {
            data->highest_bid = new_bid;
            data->highest_bidder_id = bidder_id;
        }

        if (data->bid_count < MAX_BIDS) {
            bid_record_t *record = &data->history[data->bid_count++];
            record->round = round;
            record->bidder_id = bidder_id;
            record->amount = new_bid;
            record->accepted = accepted;
        }

        if (unlock_memory(semid) == -1) {
            perror("semop unlock");
            break;
        }

        if (accepted) {
            printf("Bidder %d placed a new highest bid of %d in round %d.\n",
                   bidder_id, new_bid, round);
        } else {
            printf("Bidder %d's bid of %d was too low in round %d.\n",
                   bidder_id, new_bid, round);
        }
        fflush(stdout);
        last_round = round;
    }

    unregister_bidder(data, semid);
    shmdt(data);
    return EXIT_FAILURE;
}
