#include "bookstore.h"

book *book_head = NULL;
author *author_head = NULL;
writes *writes_head = NULL;

int book_count = 0;
int author_count = 0;
int writes_count = 0;

static void *checked_malloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL && size != 0) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

static char *copy_string(const char *text) {
    char *copy = checked_malloc(strlen(text) + 1);
    strcpy(copy, text);
    return copy;
}

static void read_line(const char *message, char *buffer, size_t size) {
    printf("%s", message);
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return;
    }
    buffer[strcspn(buffer, "\n")] = '\0';
}

static int read_int(const char *message) {
    char buffer[100];
    int value;

    while (1) {
        read_line(message, buffer, sizeof(buffer));
        if (sscanf(buffer, "%d", &value) == 1) {
            return value;
        }
        printf("Please enter a valid integer.\n");
    }
}

static float read_float(const char *message) {
    char buffer[100];
    float value;

    while (1) {
        read_line(message, buffer, sizeof(buffer));
        if (sscanf(buffer, "%f", &value) == 1) {
            return value;
        }
        printf("Please enter a valid number.\n");
    }
}

static author *find_author_by_id(int writer_id) {
    author *current = author_head;

    while (current != NULL) {
        if (current->writer_id == writer_id) {
            return current;
        }
        if (current->writer_id > writer_id) {
            return NULL;
        }
        current = current->next_author;
    }
    return NULL;
}

static author *find_first_author_by_surname(const char *surname) {
    author *current = author_head;

    while (current != NULL) {
        if (strcmp(current->surname, surname) == 0) {
            return current;
        }
        current = current->next_author;
    }
    return NULL;
}

static book *find_book_by_title(const char *title) {
    book *current = book_head;

    while (current != NULL) {
        int cmp = strcmp(current->title, title);
        if (cmp == 0) {
            return current;
        }
        if (cmp > 0) {
            return NULL;
        }
        current = current->next_book;
    }
    return NULL;
}

static int next_writer_id(void) {
    author *current = author_head;

    if (current == NULL) {
        return 1;
    }

    while (current->next_author != NULL) {
        current = current->next_author;
    }
    return current->writer_id + 1;
}

static int add_author_record(const char *surname, const char *name) {
    author *new_node = checked_malloc(sizeof(author));
    author *current;
    int id = next_writer_id();

    new_node->writer_id = id;
    new_node->surname = copy_string(surname);
    new_node->name = copy_string(name);
    new_node->num_of_books = 0;
    new_node->next_author = NULL;

    if (author_head == NULL) {
        author_head = new_node;
    } else {
        current = author_head;
        while (current->next_author != NULL) {
            current = current->next_author;
        }
        current->next_author = new_node;
    }

    author_count++;
    return id;
}

static void insert_book_node_sorted(book *new_node) {
    book *current;

    if (book_head == NULL || strcmp(new_node->title, book_head->title) < 0) {
        new_node->next_book = book_head;
        book_head = new_node;
        return;
    }

    current = book_head;
    while (current->next_book != NULL &&
           strcmp(current->next_book->title, new_node->title) < 0) {
        current = current->next_book;
    }

    new_node->next_book = current->next_book;
    current->next_book = new_node;
}

static int write_before(const writes *a, const writes *b) {
    if (a->writer_id != b->writer_id) {
        return a->writer_id < b->writer_id;
    }
    return strcmp(a->title, b->title) < 0;
}

static void insert_write_sorted(const char *title, int writer_id) {
    writes *new_node = checked_malloc(sizeof(writes));
    writes *current;

    new_node->title = copy_string(title);
    new_node->writer_id = writer_id;
    new_node->next_write = NULL;

    if (writes_head == NULL || write_before(new_node, writes_head)) {
        new_node->next_write = writes_head;
        writes_head = new_node;
    } else {
        current = writes_head;
        while (current->next_write != NULL &&
               !write_before(new_node, current->next_write)) {
            current = current->next_write;
        }
        new_node->next_write = current->next_write;
        current->next_write = new_node;
    }

    writes_count++;
}

static void remove_book_node_only(const char *title) {
    book *current = book_head;
    book *previous = NULL;

    while (current != NULL) {
        int cmp = strcmp(current->title, title);

        if (cmp == 0) {
            if (previous == NULL) {
                book_head = current->next_book;
            } else {
                previous->next_book = current->next_book;
            }
            free(current->title);
            free(current);
            book_count--;
            return;
        }
        if (cmp > 0) {
            return;
        }
        previous = current;
        current = current->next_book;
    }
}

int menu(void) {
    printf("1. Insert new writer record\n");
    printf("2. Insert new book record\n");
    printf("3. Search a writer record\n");
    printf("4. Search a book record\n");
    printf("5. Delete a writer record\n");
    printf("6. Delete a book record\n");
    printf("7. Exit\n");
    return read_int("Choice: ");
}

int insert_author(void) {
    char surname[80];
    char name[80];
    int id;

    read_line("Writer surname: ", surname, sizeof(surname));
    read_line("Writer name: ", name, sizeof(name));

    id = add_author_record(surname, name);
    printf("Writer inserted with ID %d.\n\n", id);
    return id;
}

void insert_book(void) {
    char title[120];
    int release_date;
    float price;
    int number_of_authors;
    int *author_ids;
    int i;
    book *new_book;

    read_line("Book title: ", title, sizeof(title));
    if (find_book_by_title(title) != NULL) {
        printf("A book with this title already exists.\n\n");
        return;
    }

    release_date = read_int("Release year: ");
    price = read_float("Price: ");

    do {
        number_of_authors = read_int("Number of writers: ");
        if (number_of_authors <= 0) {
            printf("A book must have at least one writer.\n");
        }
    } while (number_of_authors <= 0);

    author_ids = checked_malloc(number_of_authors * sizeof(int));

    for (i = 0; i < number_of_authors; i++) {
        char surname[80];
        author *found;

        printf("Writer %d/%d\n", i + 1, number_of_authors);
        read_line("Surname: ", surname, sizeof(surname));
        found = find_first_author_by_surname(surname);

        if (found == NULL) {
            char name[80];
            printf("Writer not found. A new writer record will be created.\n");
            read_line("Name: ", name, sizeof(name));
            author_ids[i] = add_author_record(surname, name);
        } else {
            author_ids[i] = found->writer_id;
        }
    }

    new_book = checked_malloc(sizeof(book));
    new_book->title = copy_string(title);
    new_book->release_date = release_date;
    new_book->price = price;
    new_book->next_book = NULL;
    insert_book_node_sorted(new_book);
    book_count++;

    for (i = 0; i < number_of_authors; i++) {
        author *a = find_author_by_id(author_ids[i]);
        insert_write_sorted(title, author_ids[i]);
        if (a != NULL) {
            a->num_of_books++;
        }
    }

    free(author_ids);
    printf("Book inserted successfully.\n\n");
}

void search_author(void) {
    char surname[80];
    author *current;
    int found = 0;

    read_line("Surname to search: ", surname, sizeof(surname));
    current = author_head;

    while (current != NULL) {
        if (strcmp(current->surname, surname) == 0) {
            writes *w = writes_head;
            found = 1;

            printf("\nID: %d\n", current->writer_id);
            printf("Surname: %s\n", current->surname);
            printf("Name: %s\n", current->name);
            printf("Number of books: %d\n", current->num_of_books);

            if (current->num_of_books > 0) {
                printf("Books:\n");
                while (w != NULL) {
                    if (w->writer_id == current->writer_id) {
                        book *b = find_book_by_title(w->title);
                        if (b != NULL) {
                            printf("  - %s | year: %d | price: %.2f\n",
                                   b->title, b->release_date, b->price);
                        }
                    }
                    w = w->next_write;
                }
            }
            printf("\n");
        }
        current = current->next_author;
    }

    if (!found) {
        printf("No writer with surname '%s' was found.\n\n", surname);
    }
}

void search_book(void) {
    char title[120];
    book *found;
    writes *w;

    read_line("Book title to search: ", title, sizeof(title));
    found = find_book_by_title(title);

    if (found == NULL) {
        printf("Book '%s' was not found.\n\n", title);
        return;
    }

    printf("\nTitle: %s\n", found->title);
    printf("Release year: %d\n", found->release_date);
    printf("Price: %.2f\n", found->price);
    printf("Writers:\n");

    w = writes_head;
    while (w != NULL) {
        if (strcmp(w->title, title) == 0) {
            author *a = find_author_by_id(w->writer_id);
            if (a != NULL) {
                printf("  - %s %s (ID %d)\n", a->name, a->surname, a->writer_id);
            }
        }
        w = w->next_write;
    }
    printf("\n");
}

void delete_book(void) {
    char title[120];
    book *book_to_delete;
    writes *current;
    writes *previous = NULL;

    read_line("Book title to delete: ", title, sizeof(title));
    book_to_delete = find_book_by_title(title);

    if (book_to_delete == NULL) {
        printf("Book '%s' was not found.\n\n", title);
        return;
    }

    remove_book_node_only(title);

    current = writes_head;
    while (current != NULL) {
        if (strcmp(current->title, title) == 0) {
            writes *to_delete = current;
            author *a = find_author_by_id(current->writer_id);

            if (a != NULL && a->num_of_books > 0) {
                a->num_of_books--;
            }

            if (previous == NULL) {
                writes_head = current->next_write;
                current = writes_head;
            } else {
                previous->next_write = current->next_write;
                current = previous->next_write;
            }

            free(to_delete->title);
            free(to_delete);
            writes_count--;
        } else {
            previous = current;
            current = current->next_write;
        }
    }

    printf("Book '%s' deleted successfully.\n\n", title);
}

void delete_author(void) {
    int writer_id = read_int("Writer ID to delete: ");
    author *current_author = author_head;
    author *previous_author = NULL;
    writes *current_write;
    writes *previous_write = NULL;

    while (current_author != NULL && current_author->writer_id < writer_id) {
        previous_author = current_author;
        current_author = current_author->next_author;
    }

    if (current_author == NULL || current_author->writer_id != writer_id) {
        printf("Writer with ID %d was not found.\n\n", writer_id);
        return;
    }

    current_write = writes_head;
    while (current_write != NULL) {
        if (current_write->writer_id == writer_id) {
            writes *probe = writes_head;
            int has_other_writer = 0;
            char *title_copy = copy_string(current_write->title);
            writes *to_delete = current_write;

            while (probe != NULL) {
                if (probe->writer_id != writer_id &&
                    strcmp(probe->title, title_copy) == 0) {
                    has_other_writer = 1;
                    break;
                }
                probe = probe->next_write;
            }

            if (!has_other_writer) {
                remove_book_node_only(title_copy);
            }

            if (previous_write == NULL) {
                writes_head = current_write->next_write;
                current_write = writes_head;
            } else {
                previous_write->next_write = current_write->next_write;
                current_write = previous_write->next_write;
            }

            free(to_delete->title);
            free(to_delete);
            free(title_copy);
            writes_count--;
        } else {
            previous_write = current_write;
            current_write = current_write->next_write;
        }
    }

    if (previous_author == NULL) {
        author_head = current_author->next_author;
    } else {
        previous_author->next_author = current_author->next_author;
    }

    free(current_author->surname);
    free(current_author->name);
    free(current_author);
    author_count--;

    printf("Writer with ID %d deleted successfully.\n\n", writer_id);
}

static void read_authors_file(void) {
    FILE *fp = fopen("authors.txt", "r");
    int total;
    int i;

    if (fp == NULL) {
        return;
    }

    if (fscanf(fp, "%d\n", &total) != 1 || total < 0) {
        fclose(fp);
        return;
    }

    for (i = 0; i < total; i++) {
        int id;
        char surname[120];
        char name[120];
        author *new_node;
        author *current;
        author *previous = NULL;

        fscanf(fp, "%d\n", &id);
        fgets(surname, sizeof(surname), fp);
        surname[strcspn(surname, "\n")] = '\0';
        fgets(name, sizeof(name), fp);
        name[strcspn(name, "\n")] = '\0';

        new_node = checked_malloc(sizeof(author));
        new_node->writer_id = id;
        new_node->surname = copy_string(surname);
        new_node->name = copy_string(name);
        new_node->num_of_books = 0;
        new_node->next_author = NULL;

        current = author_head;
        while (current != NULL && current->writer_id < id) {
            previous = current;
            current = current->next_author;
        }

        if (previous == NULL) {
            new_node->next_author = author_head;
            author_head = new_node;
        } else {
            new_node->next_author = current;
            previous->next_author = new_node;
        }
        author_count++;
    }

    fclose(fp);
}

static void read_books_file(void) {
    FILE *fp = fopen("books.txt", "r");
    int total;
    int i;

    if (fp == NULL) {
        return;
    }

    if (fscanf(fp, "%d\n", &total) != 1 || total < 0) {
        fclose(fp);
        return;
    }

    for (i = 0; i < total; i++) {
        char title[160];
        char line[100];
        book *new_node = checked_malloc(sizeof(book));

        fgets(title, sizeof(title), fp);
        title[strcspn(title, "\n")] = '\0';
        fgets(line, sizeof(line), fp);
        new_node->release_date = atoi(line);
        fgets(line, sizeof(line), fp);
        new_node->price = (float)atof(line);

        new_node->title = copy_string(title);
        new_node->next_book = NULL;
        insert_book_node_sorted(new_node);
        book_count++;
    }

    fclose(fp);
}

static void read_writes_file(void) {
    FILE *fp = fopen("writes.txt", "r");
    int total;
    int i;

    if (fp == NULL) {
        return;
    }

    if (fscanf(fp, "%d\n", &total) != 1 || total < 0) {
        fclose(fp);
        return;
    }

    for (i = 0; i < total; i++) {
        char title[160];
        char line[100];
        int writer_id;
        author *a;

        fgets(title, sizeof(title), fp);
        title[strcspn(title, "\n")] = '\0';
        fgets(line, sizeof(line), fp);
        writer_id = atoi(line);

        insert_write_sorted(title, writer_id);
        a = find_author_by_id(writer_id);
        if (a != NULL) {
            a->num_of_books++;
        }
    }

    fclose(fp);
}

static void write_authors_file(void) {
    FILE *fp = fopen("authors.txt", "w");
    author *current = author_head;

    if (fp == NULL) {
        perror("authors.txt");
        return;
    }

    fprintf(fp, "%d\n", author_count);
    while (current != NULL) {
        fprintf(fp, "%d\n%s\n%s\n",
                current->writer_id,
                current->surname,
                current->name);
        current = current->next_author;
    }
    fclose(fp);
}

static void write_books_file(void) {
    FILE *fp = fopen("books.txt", "w");
    book *current = book_head;

    if (fp == NULL) {
        perror("books.txt");
        return;
    }

    fprintf(fp, "%d\n", book_count);
    while (current != NULL) {
        fprintf(fp, "%s\n%d\n%.2f\n",
                current->title,
                current->release_date,
                current->price);
        current = current->next_book;
    }
    fclose(fp);
}

static void write_writes_file(void) {
    FILE *fp = fopen("writes.txt", "w");
    writes *current = writes_head;

    if (fp == NULL) {
        perror("writes.txt");
        return;
    }

    fprintf(fp, "%d\n", writes_count);
    while (current != NULL) {
        fprintf(fp, "%s\n%d\n", current->title, current->writer_id);
        current = current->next_write;
    }
    fclose(fp);
}

void load_all_data(void) {
    read_authors_file();
    read_books_file();
    read_writes_file();
}

void save_all_data(void) {
    write_authors_file();
    write_books_file();
    write_writes_file();
}

void free_all_memory(void) {
    while (author_head != NULL) {
        author *next = author_head->next_author;
        free(author_head->surname);
        free(author_head->name);
        free(author_head);
        author_head = next;
    }

    while (book_head != NULL) {
        book *next = book_head->next_book;
        free(book_head->title);
        free(book_head);
        book_head = next;
    }

    while (writes_head != NULL) {
        writes *next = writes_head->next_write;
        free(writes_head->title);
        free(writes_head);
        writes_head = next;
    }
}
