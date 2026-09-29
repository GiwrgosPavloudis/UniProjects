#ifndef AUCTION_H
#define AUCTION_H

#include <sys/types.h>
#include <sys/ipc.h>

#define AUCTION_FTOK_FILE "auction.h"
#define AUCTION_FTOK_ID 'A'
#define SEM_MUTEX 0
#define SEM_ROUND_START 1
#define SEM_COUNT 2
#define MAX_BIDS 512

#ifndef AUCTION_ROUNDS
#define AUCTION_ROUNDS 5
#endif

#ifndef ROUND_DURATION
#define ROUND_DURATION 10
#endif

typedef struct {
    int round;
    int bidder_id;
    int amount;
    int accepted;
} bid_record_t;

typedef struct {
    int highest_bid;
    int highest_bidder_id;
    int current_round;
    int round_open;
    int auction_active;
    int active_bidders;
    int bid_count;
    bid_record_t history[MAX_BIDS];
} auction_data_t;

key_t get_auction_key(void);
int lock_memory(int semid);
int unlock_memory(int semid);
int wait_for_round(int semid);
int signal_round(int semid, int count);

#endif
