
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct critbit_node critbit_node;
struct critbit_node {
    void *child[2];
    int byte;
    uint8_t otherbits;
};

critbit_root *critbit_new(void);
int critbit_insert(critbit_root *root, const char *key, const void* value);
int critbit_get(critbit_root *root, const char *key, void **out);
int critbit_contains(critbit_root *root, const char *key);
int critbit_delete(critbit_root *root, const char *key);
void critbit_clear(critbit_root *root);

#define IS_INTERNAL(ptr) (((size_t)ptr) & 1)
#define TO_NODE(ptr) ((void *)((size_t)(ptr) & ~1))
#define FROM_NODE(node) (void *)((size_t)node + 1)

#define alloc_aligned malloc

#define free_node(root, node) free(node)
#define alloc_node(root) alloc_aligned(sizeof(critbit_node))

static void* alloc_string(const char *key, int len, const void *value) {
    uint8_t *p = alloc_aligned(len + sizeof(value) + 1);
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

static int get_direction(critbit_node *node,
        const uint8_t *bytes, const size_t bytelen) {
    if(node->byte < bytelen)
        return (1 + (node->otherbits | bytes[node->byte])) >> 8;
    return 0;
}

static void *find_nearest(void *p, const uint8_t *bytes, const size_t bytelen) {
    while(IS_INTERNAL(p)) {
        critbit_node *node = TO_NODE(p);
        int dir = get_direction(node, bytes, bytelen);
        p = node->child[dir];
    }
    return p;
}

int critbit_contains(critbit_root *root, const char *key) {
    void *out = NULL;
    return critbit_get(root, key, &out);
}

#include "helper.h"

void* init(void) {
    critbit_root *root = critbit_new();
    return root;
}

int add(void *obj, const char *key, void *val) {
    critbit_root *root = obj;
    return critbit_insert(root, key, val);
}

void* find(void *obj, const char *key) {
    critbit_root *root = obj;
    void *out = NULL;
    critbit_get(root, key, &out);
    return out;
}

int del(void *obj, const char *key) {
    critbit_root *root = obj;
    return critbit_delete(root, key);
}

void clear(void *obj) {
    critbit_root *root = obj;
    critbit_clear(root);

}

void fill_depth(void *ptr, int depth, int *out, int outsize) {
    if(!IS_INTERNAL(ptr)) {
        if(depth > outsize) {
            printf("overflow: %d\n", depth);
            return;
        }
        ++out[depth];
        return;
    }

    critbit_node *node = TO_NODE(ptr);
    fill_depth(node->child[0], depth+1, out, outsize);
    fill_depth(node->child[1], depth+1, out, outsize);
}

#define DEPTH_SIZE 100
void info(void *obj) {
    critbit_root *root = obj;
    int depth_dist[DEPTH_SIZE];
    memset(depth_dist, 0, sizeof(depth_dist));
    fill_depth(root->head, 0, depth_dist, DEPTH_SIZE);

    int i;
    for(i = 0; i < DEPTH_SIZE; i++) {
        if(depth_dist[i] == 0)
            continue;
        printf("%d:\t%d\n", i, depth_dist[i]);
    }
    printf("\n");
}
