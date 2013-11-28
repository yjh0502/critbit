// Critical-bit tree implementation

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct critbit_node critbit_node;
struct critbit_node {
    void *child[2];
    uint32_t byte;
    uint8_t otherbits;
};

typedef struct critbit_root {
    void *head;
    critbit_node *freelist;
} critbit_root;

critbit_root *critbit_new(void) {
    critbit_root *root = malloc(sizeof(critbit_root));
    root->head = NULL;
    root->freelist = NULL;

    return root;
}

#define IS_INTERNAL(ptr) (((size_t)ptr) & 1)
#define TO_NODE(ptr) ((void *)((size_t)(ptr) ^ 1))

static critbit_node * alloc_node(critbit_root *root) {
    void *ret = NULL;
    if(root->freelist) {
        ret = root->freelist;
        root->freelist = root->freelist->child[0];
    } else {
        ret = malloc(sizeof(critbit_node));
    }
    return ret;
}

static void free_node(critbit_root *root, critbit_node *node) {
    node->child[0] = root->freelist;
    root->freelist = node;
}

static void * aligned_alloc(int size) {
    //XXX: Assume that malloc aligns memory on 4/8byte boundary
    return malloc(size);
/*
    void *out;
    if(posix_memalign(&out, sizeof(void *), size)) {
        return NULL;
    }
    return out;
*/
}

static int get_direction(critbit_node *node,
        const uint8_t *bytes, const size_t bytelen) {
    if(node->byte < bytelen)
        return (1 + (node->otherbits | bytes[node->byte])) >> 8;
    return 0;
}

static void *find_nearest(void *p,
        critbit_node *node, const uint8_t *bytes, const size_t bytelen) {
    while(IS_INTERNAL(p)) {
        critbit_node *node = TO_NODE(p);
        p = node->child[get_direction(node, bytes, bytelen)];
    }
    return p;
}

int critbit_contains(critbit_root *root, const char *key) {
    if(!root->head) { return 0; }

    const uint8_t *bytes = (void *)key;
    char *nearest = find_nearest(root, root->head, bytes, strlen(key));
    return strcmp(key, nearest) == 0;
}

int critbit_get(critbit_root *root, const char *key, void **out) {
    const int len = strlen(key);
    const uint8_t *bytes = (void *)key;

    char *nearest = find_nearest(root, root->head, bytes, len);

    if(strcmp(nearest, key) == 0) {
        *out = ((void *)nearest) - 1;
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

static void* cow_insert(critbit_root *root, void *p,
        const uint8_t *bytes, const size_t bytelen, const void *value) {
    critbit_node *node;
    if(!p) {
        return alloc_string((char *)bytes, bytelen, value);
    }

    if(IS_INTERNAL(p)) {
        node = TO_NODE(p);
        int dir = get_direction(node, bytes, bytelen);
        critbit_node *child = cow_insert(root, node->child[dir], bytes, bytelen, value);
        if(!child)
            return NULL;

        critbit_node *newnode = alloc_node(root);
        memcpy(newnode, node, sizeof(critbit_node));
        newnode->child[dir] = child;

        return ((char *)newnode) + 1;
    }

    // Terminal node, insert new one
    uint8_t *q = p;
    uint32_t newbyte, newotherbits;
    for(newbyte = 0; newbyte < bytelen; ++newbyte) {
        if(q[newbyte] != bytes[newbyte]) {
            newotherbits = q[newbyte] ^ bytes[newbyte];
            goto found;
        }
    }
    if(q[newbyte]) {
        newotherbits = q[newbyte];
        goto found;
    }
    return NULL;

found:
    while(newotherbits & (newotherbits - 1)) {
        newotherbits &= newotherbits - 1;
    }
    newotherbits ^= 0xFF;
    uint8_t c = q[newbyte];
    int newdirection = (1 + (newotherbits | c)) >> 8;

    critbit_node *newnode = alloc_node(root);
    if(!newnode)
        return NULL;

    char *x = alloc_string((char *)bytes, bytelen, value);
    if(!x) {
        free_node(root, newnode);
        return NULL;
    }

    newnode->byte = newbyte;
    newnode->otherbits = newotherbits;
    newnode->child[newdirection] = p;
    newnode->child[1 - newdirection] = x;

    return ((char *)newnode) + 1;
}

int critbit_insert(critbit_root *root, const char *key, const void* value) {
    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    void *newhead = cow_insert(root, root->head, bytes, len, value);
    if(!newhead)
        return 1;

    void *oldhead = root->head;
    root->head = newhead;

    // Garbage collection
    if(!oldhead)
        return 0;

    critbit_node *oldnode = NULL, *newnode = NULL;
    while(IS_INTERNAL(oldhead)) {
        oldnode = TO_NODE(oldhead);
        newnode = TO_NODE(newhead);

        if(oldnode->child[0] != newnode->child[0]) {
            oldhead = oldnode->child[0];
            newhead = newnode->child[0];
            free_node(root, oldnode);
        } else if(oldnode->child[1] != newnode->child[1]) {
            oldhead = oldnode->child[1];
            newhead = newnode->child[1];
            free_node(root, oldnode);
        }
    }

    return 0;
}

int critbit_delete(critbit_root *root, const char *key) {
    if(!root->head) return 1;

    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    uint8_t *p = root->head;
    void **wherep = &root->head, **whereq = 0;
    critbit_node *q = NULL;
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
        root->head = NULL;
        return 0;
    }

    *whereq = q->child[1 - direction];
    free(q);

    return 0;
}

static void clear_node(void *p) {
    if(IS_INTERNAL(p)) {
        // Internal node
        critbit_node *node = TO_NODE(p);
        clear_node(node->child[0]);
        clear_node(node->child[1]);
        free(node);
    } else {
        free_string(p);
    }
}

void critbit_clear(critbit_root *root) {
    if(!root->head)
        return;

    clear_node(root->head);
    root->head = NULL;
}

#include "helper.h"

void* init(void) {
    critbit_root *root = malloc(sizeof(critbit_root));
    root->head = NULL;
    return root;
}

int add(void *obj, const char *key, void *val) {
    critbit_root *root = obj;
    return critbit_insert(root, key, val);
}

void* find(void *obj, const char *key) {
    critbit_root *root = obj;
    void* out = NULL;
    critbit_get(root, key, &out);
    //printf("find returns: %d\n", ret);
    return out;
}

int del(void *obj, const char *key) {
    critbit_root *root = obj;
    return critbit_delete(root, key);
}

void clear(void *obj) {
    critbit_root *root = obj;
    critbit_clear(root);
    free(root);
}

/*
int main(void) {
    void *map = init();
    add(map, "hello", (void *)(size_t)1);
    add(map, "hell2", (void *)(size_t)2);
    add(map, "hellp", (void *)(size_t)3);

    printf("%lu\n", (size_t)find(map, "hello"));
    printf("%lu\n", (size_t)find(map, "hell2"));
    printf("%lu\n", (size_t)find(map, "hellp"));

    clear(map);

    return 0;
}
*/
