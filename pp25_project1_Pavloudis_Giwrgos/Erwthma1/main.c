#include "bookstore.h"

int main(void) {
    int choice;

    load_all_data();

    do {
        choice = menu();

        switch (choice) {
            case 1:
                insert_author();
                break;
            case 2:
                insert_book();
                break;
            case 3:
                search_author();
                break;
            case 4:
                search_book();
                break;
            case 5:
                delete_author();
                break;
            case 6:
                delete_book();
                break;
            case 7:
                save_all_data();
                free_all_memory();
                printf("Data saved. Program terminated.\n");
                break;
            default:
                printf("Invalid option. Please choose 1-7.\n\n");
        }
    } while (choice != 7);

    return 0;
}
