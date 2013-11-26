
#include "uthash.h"
#include "helper.h"

#include <stdio.h>

struct hash_entry {
    UT_hash_handle hh;
    int value;
    char key[0];
};

struct hash_entry *head = NULL;

void set(int iter) {
    int i;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);
        int len = strlen(buf);

        struct hash_entry *entry = malloc(sizeof(struct hash_entry) + len + 1);
        entry->value = i;
        strcpy(entry->key, buf);

        HASH_ADD_STR(head, key, entry);
    }
}

void get(int iter) {
    int i;
    struct hash_entry *entry;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);

        HASH_FIND_STR(head, buf, entry);
        if(entry->value != i) {
            printf("invalid value: %d != %d\n", i, entry->value);
            exit(-1);
        }
    }
}

void cleanup(int iter) {
    struct hash_entry *entry, *tmp;
    HASH_ITER(hh, head, entry, tmp) {
        free(entry);
    }
}

int main(void) {
    measure(set, iter);
    measure(get, iter);
    measure(cleanup, iter);
    return 0;
}
