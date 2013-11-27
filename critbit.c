// Critical-bit tree implementation

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct critbit_node {
    void *child[2];
    uint32_t byte;
    uint8_t otherbits;
};

typedef void* critbit_head;

#define IS_INTERNAL(ptr) (((size_t)(ptr)) & 1)
#define TO_NODE(ptr) ((void *)((size_t)(ptr) ^ 1))

static void * aligned_alloc(int size) {
    //XXX: Assume that malloc aligns memory on 4/8byte boundary
    return malloc(size);

    /*
    void *out;
    if(posix_memalign(&out, sizeof(void *), size))
        return NULL;
    return out;
    */
}

static int get_direction(struct critbit_node *node,
        const uint8_t *bytes, const size_t bytelen) {
    if(node->byte < bytelen)
        return (1 + (node->otherbits | bytes[node->byte])) >> 8;
    return 0;
}

static void *find_nearest(void *p, const uint8_t *bytes, const size_t bytelen) {
    while(IS_INTERNAL(p)) {
        struct critbit_node *q = TO_NODE(p);
        p = q->child[get_direction(q, bytes, bytelen)];
    }
    return p;
}

int critbit_contains(critbit_head *head, const char *key) {
    if(!*head) { return 0; }

    const uint8_t *bytes = (void *)key;
    return strcmp(key, find_nearest(*head, bytes, strlen(key))) == 0;
}

int critbit_get(critbit_head *head, const char *key, void **out) {
    const int len = strlen(key);
    const uint8_t *bytes = (void *)key;

    uint8_t *nearest = find_nearest(*head, bytes, len);


    if(strcmp((char *)nearest, key) == 0) {
        *out = *(void **)(nearest - sizeof(void *));
        return 0;
    }
    return 1;
}

static void* alloc_string(const char *key, int len, const void *value) {
    uint8_t *p = aligned_alloc(len + sizeof(value) + 1);
    if(!p)
        return NULL;

    memcpy(p, &value, sizeof(value));
    p += sizeof(value);
    memcpy(p, key, len + 1);

    return p;
}

static void free_string(uint8_t *key) {
    free(key - sizeof(void *));
}

int critbit_insert(critbit_head *head, const char *key, const void* value) {
    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    uint8_t *p = *head;
    if(!p) {
        p = alloc_string(key, len, value);
        if(!p)
            return 1;

        *head = p;
        return 0;
    }

    p = find_nearest(*head, bytes, len);
    
    uint32_t newbyte, newotherbits;
    for(newbyte = 0; newbyte < len; ++newbyte) {
        if(p[newbyte] != bytes[newbyte]) {
            newotherbits = p[newbyte] ^ bytes[newbyte];
            goto found;
        }
    }
    if(p[newbyte]) {
        newotherbits = p[newbyte];
        goto found;
    }
    return 1;

found:
    while(newotherbits & (newotherbits - 1)) {
        newotherbits &= newotherbits - 1;
    }
    newotherbits ^= 0xFF;
    uint8_t c = p[newbyte];
    int newdirection = (1 + (newotherbits | c)) >> 8;

    struct critbit_node *node = aligned_alloc(sizeof(struct critbit_node));
    if(!node)
        return 1;

    char *x = alloc_string(key, len, value);
    if(!x) {
        free(node);
        return 1;
    }

    node->byte = newbyte;
    node->otherbits = newotherbits;
    node->child[1 - newdirection] = x;

    void **wherep = head;
    for(;;) {
        uint8_t *p = *wherep;
        if(!IS_INTERNAL(p))
            break;

        struct critbit_node *q = TO_NODE(p);
        if(q->byte > newbyte)
            break;
        if(q->byte == newbyte && q->otherbits > newotherbits)
            break;

        wherep = q->child + get_direction(q, bytes, len);
    }
    node->child[newdirection] = *wherep;
    *wherep = (void *)(1 + (size_t)node);

    return 0;
}

int critbit_delete(critbit_head *head, const char *key) {
    if(!*head) return 1;

    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    uint8_t *p = *head;
    void **wherep = head, **whereq = 0;
    struct critbit_node *q = NULL;
    int direction = 0;

    while(IS_INTERNAL(p)) {
        whereq = wherep;
        q = TO_NODE(p);
        wherep = q->child + get_direction(q, bytes, len);
        p = *wherep;
    }

    if(strcmp(key, (const char *)p))
        return 1;

    free_string(p);
    if(!whereq) {
        *head = NULL;
        return 0;
    }

    *whereq = q->child[1 - direction];
    free(q);

    return 0;
}

static void clear_node(void *p) {
    if(IS_INTERNAL(p)) {
        // Internal node
        struct critbit_node *node = TO_NODE(p);
        clear_node(node->child[0]);
        clear_node(node->child[1]);
        free(node);
    } else {
        free_string(p);
    }
}

void critbit_clear(critbit_head *head) {
    if(!*head)
        return;

    clear_node(*head);
    head = NULL;
}

#include "helper.h"

void* init(void) {
    critbit_head *head = malloc(sizeof(critbit_head));
    return head;
}

int add(void *obj, const char *key, void *val) {
    critbit_head *head = obj;
    return critbit_insert(head, key, val);
}

void* find(void *obj, const char *key) {
    critbit_head *head = obj;
    void* out = NULL;
    critbit_get(head, key, &out);
    return out;
}

int del(void *obj, const char *key) {
    critbit_head *head = obj;
    return critbit_delete(head, key);
}

void clear(void *obj) {
    critbit_head *head = obj;
    critbit_clear(head);
}

