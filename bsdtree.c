
#include "tree.h"
#include "helper.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct hash_entry {
    RB_ENTRY(hash_entry) entries;
    const char *key;
    void *value;
};

int hash_cmp(struct hash_entry *h1, struct hash_entry *h2) {
    return strcmp(h1->key, h2->key);
}

struct hash_root {
    RB_HEAD(hash_head, hash_entry) head;
};

RB_PROTOTYPE(hash_head, hash_entry, entries, hash_cmp);
RB_GENERATE(hash_head, hash_entry, entries, hash_cmp);

void* init(void) {
    struct hash_root *root = malloc(sizeof(struct hash_root));
    return root;
}

int add(void *obj, const char *key, void *val) {
    struct hash_root *root = obj;
    if(find(obj, key)) {
        return 1;
    }

    int len = strlen(key);
    struct hash_entry *entry = malloc(sizeof(struct hash_entry) + len + 1);
    entry->key = (char *)entry + sizeof(struct hash_entry);
    entry->value = val;
    strcpy((char *)entry->key, key);

    RB_INSERT(hash_head, &root->head, entry);
    return 0;
}

void* find(void *obj, const char *key) {
    struct hash_root *root = obj;
    struct hash_entry *out, find;
    find.key = key;

    out = RB_FIND(hash_head, &root->head, &find);
    if(!out) {
        return NULL;
    }
    return out->value;
}

int del(void *obj, const char *key) {
    return 0;
}

void clear(void *obj) {
    struct hash_root *root = obj;
    struct hash_entry *entry, *tmp;
    RB_FOREACH_SAFE(entry, hash_head, &root->head, tmp) {
        free(entry);
    }
}
