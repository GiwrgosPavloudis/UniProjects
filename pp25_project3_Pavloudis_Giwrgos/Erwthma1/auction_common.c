#include "auction.h"

#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>

key_t get_auction_key(void) {
    return ftok(AUCTION_FTOK_FILE, AUCTION_FTOK_ID);
}

static int sem_change(int semid, unsigned short sem_num, short change) {
    struct sembuf operation;
    operation.sem_num = sem_num;
    operation.sem_op = change;
    operation.sem_flg = 0;

    while (semop(semid, &operation, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }
        return -1;
    }
    return 0;
}

int lock_memory(int semid) {
    return sem_change(semid, SEM_MUTEX, -1);
}

int unlock_memory(int semid) {
    return sem_change(semid, SEM_MUTEX, 1);
}

int wait_for_round(int semid) {
    return sem_change(semid, SEM_ROUND_START, -1);
}

int signal_round(int semid, int count) {
    if (count <= 0 || count > 30000) {
        errno = EINVAL;
        return -1;
    }
    return sem_change(semid, SEM_ROUND_START, (short)count);
}
