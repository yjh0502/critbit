// Critical-bit tree implementation

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef union critbit_node critbit_node;
union critbit_node {
    struct {
        uint32_t child[2];
        uint32_t handle;
        uint16_t byte;
        uint8_t otherbits;
        uint8_t padding;
    } internal;

    struct {
        void *value;
        char *key;
    } leaf;

    struct {
        uint32_t cur, next;
    } free;
};

// 4 kilobytes
typedef struct critbit_arena {
    critbit_node nodes[256];
} critbit_arena;

typedef struct critbit_root {
    uint32_t head;

    uint32_t arena_head;
    int arena_len;
    critbit_arena **arenas;
} critbit_root;


static int is_internal(critbit_node *node) {
    return node->internal.padding & 1;
}

static critbit_arena * arena_new(int arena_id, uint32_t next) {
    critbit_arena *arena = malloc(sizeof(critbit_arena));
    if(!arena)
        return NULL;

    int i;
    for(i = 1; i < 256; i++) {
        arena->nodes[i].free.cur = (arena_id << 8) + i;
        arena->nodes[i].free.next = (arena_id << 8) + i + 1;
    }
    arena->nodes[255].free.next = next;
    return arena;
}

static critbit_node *arena_get(critbit_root* root, uint32_t handle) {
    int arena_id = handle / 256;
    int arena_idx = handle % 256;

    printf("get: %d %d\n", arena_id, arena_idx);

    return &root->arenas[arena_id]->nodes[arena_idx];
}

static int add_arena(critbit_root *root) {
    int arena_id = root->arena_len;
    root->arenas = realloc(root->arenas, sizeof(critbit_arena *) * (arena_id + 1));
    root->arenas[arena_id] = arena_new(arena_id, root->arena_head);
    ++root->arena_len;

    root->arena_head = (arena_id << 8) + 1;
    printf("arena_id: %d, new head: %d\n", arena_id, root->arena_head);
    return 0;
}

static critbit_node *arena_alloc(critbit_root *root, uint32_t *handle_out) {
    printf("current head: %d\n", root->arena_head);
    critbit_node *ret = arena_get(root, root->arena_head);
    root->arena_head = ret->free.next;

    printf("arena alloc: %d\n", ret->free.cur);
    *handle_out = ret->free.cur;

    if(!root->arena_head) {
        printf("Allocate new arena\n");
        add_arena(root);
    }

    return ret;
}

static void arena_free(critbit_root *root, uint32_t handle) {
    printf("free: %d\n", handle);
    critbit_node *node = arena_get(root, handle);
    node->free.cur = handle;
    node->free.next = root->arena_head;
    root->arena_head = handle;
}

critbit_root *critbit_new(void) {
    critbit_root *root = malloc(sizeof(critbit_root));
    root->head = 0;
    root->arena_head = 0;
    root->arena_len = 0;
    root->arenas = NULL;

    add_arena(root);

    return root;
}


static void * aligned_alloc(int size) {
    //XXX: Assume that malloc aligns memory on 4/8byte boundary
    void * addr = malloc(size);
    printf("aligned_alloc: %p\n", addr);
    return addr;

    /*
    void *out;
    if(posix_memalign(&out, sizeof(void *), size))
        return NULL;
    return out;
    */
}

static int get_direction(critbit_node *node,
        const uint8_t *bytes, const size_t bytelen) {
    if(node->internal.byte < bytelen)
        return (1 + (node->internal.otherbits | bytes[node->internal.byte])) >> 8;
    return 0;
}

static uint32_t find_nearest(critbit_root *root,
        uint32_t handle, const uint8_t *bytes, const size_t bytelen) {
    critbit_node *node = arena_get(root, handle);
    while(is_internal(node)) {
        handle = node->internal.child[get_direction(node, bytes, bytelen)];
        node = arena_get(root, handle);
    }
    return handle;
}

/*
int critbit_contains(critbit_root *root, const char *key) {
    if(!root->head) { return 0; }

    const uint8_t *bytes = (void *)key;
    critbit_node *nearest = find_nearest(root, root->head, bytes, strlen(key));
    return strcmp(key, nearest->leaf.key) == 0;
}
*/

int critbit_get(critbit_root *root, const char *key, void **out) {
    const int len = strlen(key);
    const uint8_t *bytes = (void *)key;

    critbit_node *nearest = arena_get(root, find_nearest(root, root->head, bytes, len));
    printf("critbit_get: %s\n", nearest->leaf.key);

    if(strcmp(nearest->leaf.key, key) == 0) {
        *out = nearest->leaf.value;
        return 0;
    }
    return 1;
}

static uint32_t alloc_leaf(critbit_root *root,
        const uint8_t *bytes, const size_t bytelen, const void *value) {
    uint32_t handle = 0;
    critbit_node *node = arena_alloc(root, &handle);
    //printf("node: %p\n", node);
    node->leaf.key = (char *)aligned_alloc(bytelen + 1);
    memcpy(node->leaf.key, bytes, bytelen + 1);
    node->leaf.value = (void *)value;

    return handle;
}

static int cow_insert(critbit_root *root, uint32_t handle,
        const uint8_t *bytes, const size_t bytelen, const void *value,
        uint32_t *handle_out) {
    critbit_node *node;
    if(handle == 0) {
        *handle_out = alloc_leaf(root, bytes, bytelen, value);
        return 0;
    }
    printf("insert: handle: %u\n", handle);

    node = arena_get(root, handle);
    if(is_internal(node)) {
        int dir = get_direction(node, bytes, bytelen);

        uint32_t child_handle;
        if(cow_insert(root, node->internal.child[dir],
                bytes, bytelen, value, &child_handle)) {
            return 1;
        }
        printf("cow handle: %d\n", child_handle);

        uint32_t newhandle = 0;
        critbit_node *newnode = arena_alloc(root, &newhandle);
        memcpy(newnode, node, sizeof(critbit_node));
        newnode->internal.child[dir] = child_handle;

        *handle_out = newhandle;
        return 0;
    }

    // Terminal node, insert new one
    uint8_t *q = (uint8_t *)node->leaf.key;
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
    return 1;

found:
    while(newotherbits & (newotherbits - 1)) {
        newotherbits &= newotherbits - 1;
    }
    newotherbits ^= 0xFF;
    uint8_t c = q[newbyte];
    int newdirection = (1 + (newotherbits | c)) >> 8;

    uint32_t newhandle;
    critbit_node *newnode = arena_alloc(root, &newhandle);
    if(!newnode) {
        return 1;
    }

    newnode->internal.byte = newbyte;
    newnode->internal.otherbits = newotherbits;
    newnode->internal.padding = 1;
    newnode->internal.child[1 - newdirection] = alloc_leaf(root, bytes, bytelen, value);
    newnode->internal.child[newdirection] = handle;

    *handle_out = newhandle;
    return 0;
}

int critbit_insert(critbit_root *root, const char *key, const void* value) {
    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    critbit_node *newnode = NULL;
    uint32_t handle = 0, oldhandle = 0;
    if(cow_insert(root, root->head, bytes, len, value, &handle)) {
        return 1;
    }
    newnode = arena_get(root, handle);
    if(!newnode)
        return 1;

    oldhandle = root->head;
    root->head = handle;

    // Garbage collection
    if(oldhandle == 0)
        return 0;
    critbit_node *oldnode = arena_get(root, oldhandle);

    while(is_internal(oldnode)) {
        if(oldnode->internal.child[0] != newnode->internal.child[0]) {
            oldnode = arena_get(root, oldnode->internal.child[0]);
            newnode = arena_get(root, newnode->internal.child[0]);

            handle = oldnode->internal.child[0];
            arena_free(root, oldhandle);
            oldhandle = handle;
        } else if(oldnode->internal.child[1] != newnode->internal.child[1]) {
            oldnode = arena_get(root, oldnode->internal.child[1]);
            newnode = arena_get(root, newnode->internal.child[1]);

            handle = oldnode->internal.child[1];
            arena_free(root, oldhandle);
            oldhandle = handle;
        }
    }

    return 0;
}

int critbit_delete(critbit_root *root, const char *key) {
    /*
    if(!root->head) return 1;

    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    uint8_t *p = root->head;
    void **wherep = &root->head, **whereq = 0;
    critbit_node *q = NULL;
    int direction = 0;

    while(is_internal(p)) {
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
    */

    return 0;
}

static void clear_node(critbit_root *root, uint32_t handle) {
    critbit_node *node = arena_get(root, handle);
    if(is_internal(node)) {
        // Internal node
        clear_node(root, node->internal.child[0]);
        clear_node(root, node->internal.child[1]);
        arena_free(root, handle);
    } else {
        free(node->leaf.key);
        arena_free(root, handle);
    }
}

void critbit_clear(critbit_root *root) {
    if(root->head) {
        clear_node(root, root->head);
        root->head = UINT32_MAX;
    }
    if(root->arenas) {
        free(root->arenas[0]);
        free(root->arenas);
        root->arena_head = 0;
    }
}

#include "helper.h"

void* init(void) {
    return critbit_new();
}

int add(void *obj, const char *key, void *val) {
    critbit_root *root = obj;
    return critbit_insert(root, key, val);
}

void* find(void *obj, const char *key) {
    critbit_root *root = obj;
    void* out = NULL;
    /* int ret =*/ critbit_get(root, key, &out);
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

    printf("%lu\n", (size_t)find(map, "hellp"));
    printf("%lu\n", (size_t)find(map, "hello"));
    printf("%lu\n", (size_t)find(map, "hell2"));

    del(map, "hello");
    del(map, "hell2");
    del(map, "hellp");

    clear(map);

    return 0;
}
*/
