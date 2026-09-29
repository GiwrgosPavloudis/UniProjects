#include "bookstore.h"

book *book_arr = NULL;
author *author_arr = NULL;
writes *writes_arr = NULL;

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

static void *checked_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size != 0) {
        fprintf(stderr, "Memory reallocation failed.\n");
        exit(EXIT_FAILURE);
    }
    return new_ptr;
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

static int compare_authors_by_id(const void *a, const void *b) {
    const author *x = (const author *)a;
    const author *y = (const author *)b;
    return (x->writer_id > y->writer_id) - (x->writer_id < y->writer_id);
}

static int compare_books_by_title(const void *a, const void *b) {
    const book *x = (const book *)a;
    const book *y = (const book *)b;
    return strcmp(x->title, y->title);
}

static int compare_writes(const void *a, const void *b) {
    const writes *x = (const writes *)a;
    const writes *y = (const writes *)b;

    if (x->writer_id != y->writer_id) {
        return (x->writer_id > y->writer_id) - (x->writer_id < y->writer_id);
    }
    return strcmp(x->title, y->title);
}

static int author_index_by_id(int writer_id) {
    int left = 0;
    int right = author_count - 1;

    while (left <= right) {
        int middle = left + (right - left) / 2;

        if (author_arr[middle].writer_id == writer_id) {
            return middle;
        }
        if (author_arr[middle].writer_id < writer_id) {
            left = middle + 1;
        } else {
            right = middle - 1;
        }
    }
    return -1;
}

static int book_index_by_title(const char *title) {
    int left = 0;
    int right = book_count - 1;

    while (left <= right) {
        int middle = left + (right - left) / 2;
        int cmp = strcmp(book_arr[middle].title, title);

        if (cmp == 0) {
            return middle;
        }
        if (cmp < 0) {
            left = middle + 1;
        } else {
            right = middle - 1;
        }
    }
    return -1;
}

static int first_author_index_by_surname(const char *surname) {
    int i;
    for (i = 0; i < author_count; i++) {
        if (strcmp(author_arr[i].surname, surname) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_author_record(const char *surname, const char *name) {
    int new_id = (author_count == 0) ? 1 : author_arr[author_count - 1].writer_id + 1;

    author_arr = checked_realloc(author_arr, (author_count + 1) * sizeof(author));
    author_arr[author_count].writer_id = new_id;
    author_arr[author_count].surname = copy_string(surname);
    author_arr[author_count].name = copy_string(name);
    author_arr[author_count].num_of_books = 0;
    author_count++;

    return new_id;
}

static void add_write_record(const char *title, int writer_id) {
    int pos = writes_count;

    writes_arr = checked_realloc(writes_arr, (writes_count + 1) * sizeof(writes));

    while (pos > 0) {
        writes previous;
        previous.title = (char *)title;
        previous.writer_id = writer_id;

        if (compare_writes(&writes_arr[pos - 1], &previous) <= 0) {
            break;
        }
        writes_arr[pos] = writes_arr[pos - 1];
        pos--;
    }

    writes_arr[pos].title = copy_string(title);
    writes_arr[pos].writer_id = writer_id;
    writes_count++;
}

static void shrink_authors(void) {
    if (author_count == 0) {
        free(author_arr);
        author_arr = NULL;
    } else {
        author_arr = checked_realloc(author_arr, author_count * sizeof(author));
    }
}

static void shrink_books(void) {
    if (book_count == 0) {
        free(book_arr);
        book_arr = NULL;
    } else {
        book_arr = checked_realloc(book_arr, book_count * sizeof(book));
    }
}

static void shrink_writes(void) {
    if (writes_count == 0) {
        free(writes_arr);
        writes_arr = NULL;
    } else {
        writes_arr = checked_realloc(writes_arr, writes_count * sizeof(writes));
    }
}

static void remove_book_record_only(const char *title) {
    int pos = book_index_by_title(title);
    int i;

    if (pos == -1) {
        return;
    }

    free(book_arr[pos].title);
    for (i = pos; i < book_count - 1; i++) {
        book_arr[i] = book_arr[i + 1];
    }
    book_count--;
    shrink_books();
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
    int insert_pos;

    read_line("Book title: ", title, sizeof(title));

    if (book_index_by_title(title) != -1) {
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
        int index;

        printf("Writer %d/%d\n", i + 1, number_of_authors);
        read_line("Surname: ", surname, sizeof(surname));
        index = first_author_index_by_surname(surname);

        if (index == -1) {
            char name[80];
            printf("Writer not found. A new writer record will be created.\n");
            read_line("Name: ", name, sizeof(name));
            author_ids[i] = add_author_record(surname, name);
        } else {
            author_ids[i] = author_arr[index].writer_id;
        }
    }

    book_arr = checked_realloc(book_arr, (book_count + 1) * sizeof(book));
    insert_pos = book_count;
    while (insert_pos > 0 && strcmp(book_arr[insert_pos - 1].title, title) > 0) {
        book_arr[insert_pos] = book_arr[insert_pos - 1];
        insert_pos--;
    }

    book_arr[insert_pos].title = copy_string(title);
    book_arr[insert_pos].release_date = release_date;
    book_arr[insert_pos].price = price;
    book_count++;

    for (i = 0; i < number_of_authors; i++) {
        int author_pos = author_index_by_id(author_ids[i]);
        add_write_record(title, author_ids[i]);
        if (author_pos != -1) {
            author_arr[author_pos].num_of_books++;
        }
    }

    free(author_ids);
    printf("Book inserted successfully.\n\n");
}

void search_author(void) {
    char surname[80];
    int i;
    int found = 0;

    read_line("Surname to search: ", surname, sizeof(surname));

    for (i = 0; i < author_count; i++) {
        if (strcmp(author_arr[i].surname, surname) == 0) {
            int j;
            found = 1;
            printf("\nID: %d\n", author_arr[i].writer_id);
            printf("Surname: %s\n", author_arr[i].surname);
            printf("Name: %s\n", author_arr[i].name);
            printf("Number of books: %d\n", author_arr[i].num_of_books);

            if (author_arr[i].num_of_books > 0) {
                printf("Books:\n");
                for (j = 0; j < writes_count; j++) {
                    if (writes_arr[j].writer_id == author_arr[i].writer_id) {
                        int b = book_index_by_title(writes_arr[j].title);
                        if (b != -1) {
                            printf("  - %s | year: %d | price: %.2f\n",
                                   book_arr[b].title,
                                   book_arr[b].release_date,
                                   book_arr[b].price);
                        }
                    }
                }
            }
            printf("\n");
        }
    }

    if (!found) {
        printf("No writer with surname '%s' was found.\n\n", surname);
    }
}

void search_book(void) {
    char title[120];
    int pos;
    int i;

    read_line("Book title to search: ", title, sizeof(title));
    pos = book_index_by_title(title);

    if (pos == -1) {
        printf("Book '%s' was not found.\n\n", title);
        return;
    }

    printf("\nTitle: %s\n", book_arr[pos].title);
    printf("Release year: %d\n", book_arr[pos].release_date);
    printf("Price: %.2f\n", book_arr[pos].price);
    printf("Writers:\n");

    for (i = 0; i < writes_count; i++) {
        if (strcmp(writes_arr[i].title, title) == 0) {
            int a = author_index_by_id(writes_arr[i].writer_id);
            if (a != -1) {
                printf("  - %s %s (ID %d)\n",
                       author_arr[a].name,
                       author_arr[a].surname,
                       author_arr[a].writer_id);
            }
        }
    }
    printf("\n");
}

void delete_book(void) {
    char title[120];
    int pos;
    int i = 0;
    int j;

    read_line("Book title to delete: ", title, sizeof(title));
    pos = book_index_by_title(title);

    if (pos == -1) {
        printf("Book '%s' was not found.\n\n", title);
        return;
    }

    while (i < writes_count) {
        if (strcmp(writes_arr[i].title, title) == 0) {
            int a = author_index_by_id(writes_arr[i].writer_id);
            if (a != -1 && author_arr[a].num_of_books > 0) {
                author_arr[a].num_of_books--;
            }

            free(writes_arr[i].title);
            for (j = i; j < writes_count - 1; j++) {
                writes_arr[j] = writes_arr[j + 1];
            }
            writes_count--;
        } else {
            i++;
        }
    }
    shrink_writes();

    free(book_arr[pos].title);
    for (i = pos; i < book_count - 1; i++) {
        book_arr[i] = book_arr[i + 1];
    }
    book_count--;
    shrink_books();

    printf("Book '%s' deleted successfully.\n\n", title);
}

void delete_author(void) {
    int writer_id = read_int("Writer ID to delete: ");
    int author_pos = author_index_by_id(writer_id);
    char **monographs = NULL;
    int monograph_count = 0;
    int i;
    int j;

    if (author_pos == -1) {
        printf("Writer with ID %d was not found.\n\n", writer_id);
        return;
    }

    for (i = 0; i < writes_count; i++) {
        if (writes_arr[i].writer_id == writer_id) {
            int has_other_writer = 0;

            for (j = 0; j < writes_count; j++) {
                if (j != i &&
                    strcmp(writes_arr[j].title, writes_arr[i].title) == 0 &&
                    writes_arr[j].writer_id != writer_id) {
                    has_other_writer = 1;
                    break;
                }
            }

            if (!has_other_writer) {
                monographs = checked_realloc(monographs,
                                             (monograph_count + 1) * sizeof(char *));
                monographs[monograph_count++] = copy_string(writes_arr[i].title);
            }
        }
    }

    i = 0;
    while (i < writes_count) {
        if (writes_arr[i].writer_id == writer_id) {
            free(writes_arr[i].title);
            for (j = i; j < writes_count - 1; j++) {
                writes_arr[j] = writes_arr[j + 1];
            }
            writes_count--;
        } else {
            i++;
        }
    }
    shrink_writes();

    for (i = 0; i < monograph_count; i++) {
        remove_book_record_only(monographs[i]);
        free(monographs[i]);
    }
    free(monographs);

    free(author_arr[author_pos].surname);
    free(author_arr[author_pos].name);
    for (i = author_pos; i < author_count - 1; i++) {
        author_arr[i] = author_arr[i + 1];
    }
    author_count--;
    shrink_authors();

    printf("Writer with ID %d deleted successfully.\n\n", writer_id);
}

static void read_authors_file(void) {
    FILE *fp = fopen("authors.txt", "r");
    int i;

    if (fp == NULL) {
        return;
    }

    if (fscanf(fp, "%d\n", &author_count) != 1 || author_count < 0) {
        fclose(fp);
        author_count = 0;
        return;
    }

    author_arr = checked_malloc(author_count * sizeof(author));

    for (i = 0; i < author_count; i++) {
        char surname[120];
        char name[120];

        fscanf(fp, "%d\n", &author_arr[i].writer_id);
        fgets(surname, sizeof(surname), fp);
        surname[strcspn(surname, "\n")] = '\0';
        fgets(name, sizeof(name), fp);
        name[strcspn(name, "\n")] = '\0';

        author_arr[i].surname = copy_string(surname);
        author_arr[i].name = copy_string(name);
        author_arr[i].num_of_books = 0;
    }

    fclose(fp);
    qsort(author_arr, author_count, sizeof(author), compare_authors_by_id);
}

static void read_books_file(void) {
    FILE *fp = fopen("books.txt", "r");
    int i;

    if (fp == NULL) {
        return;
    }

    if (fscanf(fp, "%d\n", &book_count) != 1 || book_count < 0) {
        fclose(fp);
        book_count = 0;
        return;
    }

    book_arr = checked_malloc(book_count * sizeof(book));

    for (i = 0; i < book_count; i++) {
        char title[160];
        char number_line[100];

        fgets(title, sizeof(title), fp);
        title[strcspn(title, "\n")] = '\0';
        fgets(number_line, sizeof(number_line), fp);
        book_arr[i].release_date = atoi(number_line);
        fgets(number_line, sizeof(number_line), fp);
        book_arr[i].price = (float)atof(number_line);
        book_arr[i].title = copy_string(title);
    }

    fclose(fp);
    qsort(book_arr, book_count, sizeof(book), compare_books_by_title);
}

static void read_writes_file(void) {
    FILE *fp = fopen("writes.txt", "r");
    int i;

    if (fp == NULL) {
        return;
    }

    if (fscanf(fp, "%d\n", &writes_count) != 1 || writes_count < 0) {
        fclose(fp);
        writes_count = 0;
        return;
    }

    writes_arr = checked_malloc(writes_count * sizeof(writes));

    for (i = 0; i < writes_count; i++) {
        char title[160];
        char id_line[100];

        fgets(title, sizeof(title), fp);
        title[strcspn(title, "\n")] = '\0';
        fgets(id_line, sizeof(id_line), fp);

        writes_arr[i].title = copy_string(title);
        writes_arr[i].writer_id = atoi(id_line);
    }

    fclose(fp);
    qsort(writes_arr, writes_count, sizeof(writes), compare_writes);

    for (i = 0; i < writes_count; i++) {
        int a = author_index_by_id(writes_arr[i].writer_id);
        if (a != -1) {
            author_arr[a].num_of_books++;
        }
    }
}

static void write_authors_file(void) {
    FILE *fp = fopen("authors.txt", "w");
    int i;

    if (fp == NULL) {
        perror("authors.txt");
        return;
    }

    fprintf(fp, "%d\n", author_count);
    for (i = 0; i < author_count; i++) {
        fprintf(fp, "%d\n%s\n%s\n",
                author_arr[i].writer_id,
                author_arr[i].surname,
                author_arr[i].name);
    }
    fclose(fp);
}

static void write_books_file(void) {
    FILE *fp = fopen("books.txt", "w");
    int i;

    if (fp == NULL) {
        perror("books.txt");
        return;
    }

    fprintf(fp, "%d\n", book_count);
    for (i = 0; i < book_count; i++) {
        fprintf(fp, "%s\n%d\n%.2f\n",
                book_arr[i].title,
                book_arr[i].release_date,
                book_arr[i].price);
    }
    fclose(fp);
}

static void write_writes_file(void) {
    FILE *fp = fopen("writes.txt", "w");
    int i;

    if (fp == NULL) {
        perror("writes.txt");
        return;
    }

    fprintf(fp, "%d\n", writes_count);
    for (i = 0; i < writes_count; i++) {
        fprintf(fp, "%s\n%d\n",
                writes_arr[i].title,
                writes_arr[i].writer_id);
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
    int i;

    for (i = 0; i < author_count; i++) {
        free(author_arr[i].surname);
        free(author_arr[i].name);
    }
    free(author_arr);

    for (i = 0; i < book_count; i++) {
        free(book_arr[i].title);
    }
    free(book_arr);

    for (i = 0; i < writes_count; i++) {
        free(writes_arr[i].title);
    }
    free(writes_arr);

    author_arr = NULL;
    book_arr = NULL;
    writes_arr = NULL;
}
