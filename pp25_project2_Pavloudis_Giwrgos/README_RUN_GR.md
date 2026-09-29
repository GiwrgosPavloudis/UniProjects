# Εργασία 2 — Οδηγός εκτέλεσης στο VS Code

## Ερώτημα 1 — Mini shell

Άνοιξε τον φάκελο `Erwthma1` στο VS Code.

### Compile

```bash
clang -std=c11 -Wall -Wextra -Wpedantic main.c shell.c -o my_shell
```

### Run

```bash
./my_shell
```

### Tests

```text
ls
ls -l
ps
printf hello
printf hello > out.txt
cat < out.txt
printf world >> out.txt
cat out.txt
ls -l | wc -l
ls -l | wc -l > count.txt
cat count.txt
exit
```

Σημείωση: βάλε κενά γύρω από `<`, `>`, `>>`, `|`.

Η βασική έκδοση υποστηρίζει **μία σωλήνωση**, όπως απαιτεί το υποχρεωτικό μέρος. Πολλαπλές σωληνώσεις είναι bonus.

---

## Ερώτημα 2 — TCP client/server ανταλλακτηρίου

Άνοιξε τον φάκελο `Erwthma2` στο VS Code.

Χρειάζεσαι **δύο terminals**.

### Terminal 1 — compile/run server

```bash
clang -std=c11 -Wall -Wextra -Wpedantic server.c common.c -o server
./server
```

Πρέπει να δεις:

```text
Exchange server listening on port 8081.
```

### Terminal 2 — compile/run client

```bash
clang -std=c11 -Wall -Wextra -Wpedantic client.c common.c -o client
./client
```

Πρέπει να δεις σύνδεση στον server και menu.

### Βασικό test

1. Register χρήστη `alice` / `1234`.
2. Login `alice` / `1234`.
3. Create account -> type 1 (individual). Σημείωσε το Account ID.
4. Deposit -> το ID, EUR, 100.
5. Exchange -> το ID, type 1, amount 20.
6. Withdraw -> το ID, USD, 5.
7. Exit.

### Joint account test

Άνοιξε δεύτερο client terminal:

```bash
./client
```

- Client A: register `alice` αν δεν υπάρχει.
- Client B: register `bob` / `5678`.
- Client A: login `alice`, Create account -> type 2 -> second owner `bob`.
- Client B: login `bob` και χρησιμοποίησε το ίδιο Account ID για deposit/withdraw/exchange.

Το `accounts.txt` κλειδώνεται με `fcntl` κατά τις μεταβολές, ώστε δύο child processes του server να μην ενημερώνουν ταυτόχρονα τον ίδιο κοινό λογαριασμό χωρίς συγχρονισμό.

### Καθαρό reset των δεδομένων

Σταμάτα τον server και διέγραψε:

```bash
rm -f users.txt accounts.txt
```

Στην επόμενη εκκίνηση του server δημιουργούνται ξανά αυτόματα.
