#include "auction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int main(void) {
    key_t key = get_auction_key();
    if (key == (key_t)-1) {
        perror("ftok");
        return EXIT_FAILURE;
    }

    int shmid = shmget(key, sizeof(auction_data_t), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }

    auction_data_t *data = shmat(shmid, NULL, 0);
    if (data == (void *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    int semid = semget(key, SEM_COUNT, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        shmdt(data);
        return EXIT_FAILURE;
    }

    union semun arg;
    unsigned short values[SEM_COUNT] = {1, 0};
    arg.array = values;
    if (semctl(semid, 0, SETALL, arg) == -1) {
        perror("semctl SETALL");
        shmdt(data);
        return EXIT_FAILURE;
    }

    memset(data, 0, sizeof(*data));
    data->highest_bidder_id = -1;
    data->auction_active = 1;

    if (shmdt(data) == -1) {
        perror("shmdt");
        return EXIT_FAILURE;
    }

    printf("Auction IPC initialized successfully.\n");
    return EXIT_SUCCESS;
}
