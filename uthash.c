
#include "uthash.h"
#include "helper.h"

#include <stdio.h>

struct hash_entry {
    UT_hash_handle hh;
    void *value;
    char key[0];
};

struct hash_root {
    struct hash_entry *head;
};

void* init(void) {
    struct hash_root *root = malloc(sizeof(struct hash_root *));
    root->head = NULL;
    return root;
}

int add(void *obj, const char *key, void *val) {
    struct hash_root *root = obj;
    struct hash_entry *entry;

    HASH_FIND_STR(root->head, key, entry);
    if(entry)
        return 1;

    int len = strlen(key);
    entry = malloc(sizeof(struct hash_entry) + len + 1);
    entry->value = val;
    strcpy(entry->key, key);

    HASH_ADD_STR(root->head, key, entry);
    return 0;
}

void* find(void *obj, const char *key) {
    struct hash_root *root = obj;
    struct hash_entry *entry;

    HASH_FIND_STR(root->head, key, entry);
    if(entry)
        return NULL;
    return entry->value;
}

int del(void *obj, const char *key) {
    struct hash_root *root = obj;
    struct hash_entry *entry = find(obj, key);
    if(!entry) {
        return 1;
    }

    HASH_DEL(root->head, entry);
    return 0;
}

void clear(void *obj) {
    struct hash_root *root = obj;
    struct hash_entry *entry, *tmp;
    HASH_ITER(hh, root->head, entry, tmp) {
        HASH_DEL(root->head, entry);
    }
}
