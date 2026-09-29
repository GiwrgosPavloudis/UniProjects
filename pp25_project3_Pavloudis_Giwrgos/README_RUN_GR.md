# Εργασία 3 - Σύντομος οδηγός εκτέλεσης

Μπείτε στον φάκελο `Erwthma1` και κάντε compile:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic auction_init.c auction_common.c -o auction_init
gcc -std=c11 -Wall -Wextra -Wpedantic auctioneer.c auction_common.c -o auctioneer
gcc -std=c11 -Wall -Wextra -Wpedantic bidder.c auction_common.c -o bidder
```

1. Μία φορά πριν από τη δημοπρασία:
   `./auction_init`
2. Σε δεύτερο terminal, για παράδειγμα για 3 bidders:
   `./auctioneer 3`
3. Σε τρία ξεχωριστά terminals:
   `./bidder 1`
   `./bidder 2`
   `./bidder 3`

Δεν χρησιμοποιείται `fork()`. Κάθε bidder είναι ξεχωριστή διεργασία που ξεκινά από το terminal.
