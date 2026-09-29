#ifndef BOOKSTORE_H
#define BOOKSTORE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct book {
    char *title;
    int release_date;
    float price;
} book;

typedef struct author {
    int writer_id;
    char *surname;
    char *name;
    int num_of_books;
} author;

typedef struct writes {
    char *title;
    int writer_id;
} writes;

extern book *book_arr;
extern author *author_arr;
extern writes *writes_arr;

extern int book_count;
extern int author_count;
extern int writes_count;

void load_all_data(void);
void save_all_data(void);
void free_all_memory(void);

int menu(void);
int insert_author(void);
void insert_book(void);
void search_author(void);
void search_book(void);
void delete_author(void);
void delete_book(void);

#endif
