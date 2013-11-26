
#include "tree.h"
#include "helper.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct hash_entry {
    RB_ENTRY(hash_entry) entries;
    int value;
    char key[0];
};

int hash_cmp(struct hash_entry *h1, struct hash_entry *h2) {
    return strcmp(h1->key, h2->key);
}

RB_HEAD(hash_head, hash_entry) head;

RB_PROTOTYPE(hash_head, hash_entry, entries, hash_cmp);
RB_GENERATE(hash_head, hash_entry, entries, hash_cmp);

void set(int iter) {
    int i;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);
        int len = strlen(buf);

        struct hash_entry *entry = malloc(sizeof(struct hash_entry) + len + 1);
        entry->value = i;
        strcpy(entry->key, buf);

        RB_INSERT(hash_head, &head, entry);
    }
}

void get(int iter) {
    int i;
    struct hash_entry *out, *find = malloc(sizeof(struct hash_entry) + 20);
    for(i = 0; i < iter; i++) {
        sprintf(find->key, "%09d", i);


        out = RB_FIND(hash_head, &head, find);
        if(out->value != i) {
            printf("invalid value: %d != %d\n", i, out->value);
            exit(-1);
        }
    }
}

void cleanup(int iter) {
    struct hash_entry *entry, *tmp;
    RB_FOREACH_SAFE(entry, hash_head, &head, tmp) { 
        free(entry);
    }
}

int main(void) {
    measure(set, iter);
    measure(get, iter);
    measure(cleanup, iter);
    return 0;
}
