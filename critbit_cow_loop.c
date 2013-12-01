// Critical-bit tree implementation

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define NODE_CACHE
#define DEFAULT_DELETE
#include "critbit_common.h"

static void* cow_loop_insert(critbit_root *root,
        const uint8_t *bytes, const size_t bytelen, const void *value) {
    if(!root->head) {
        return alloc_string((char *)bytes, bytelen, value);
    }

    critbit_node *newhead = alloc_node(root);
    critbit_node *node = NULL, *newnode = newhead;
    void *p = root->head;
    while(IS_INTERNAL(p)) {
        node = TO_NODE(p);
        memcpy(newnode, node, sizeof(critbit_node));

        int dir = get_direction(node, bytes, bytelen);

        p = node->child[dir];
        critbit_node *next = alloc_node(root);
        newnode->child[dir] = TO_NODE(next);
        newnode = next;
    }

    // Terminal node, insert new one
    uint8_t *q = p;
    uint32_t newbyte, bits;
    for(newbyte = 0; newbyte < bytelen; ++newbyte) {
        if(q[newbyte] != bytes[newbyte]) {
            bits = q[newbyte] ^ bytes[newbyte];
            goto found;
        }
    }
    if(q[newbyte]) {
        bits = q[newbyte];
        goto found;
    }
    return NULL;

found:
    while(bits & (bits - 1))
        bits &= bits - 1;
    bits ^= 0xff;
    int dir = (1 + (bits | q[newbyte])) >> 8;

    char *x = alloc_string((char *)bytes, bytelen, value);
    if(!x) {
        return NULL;
    }

    newnode->byte = newbyte;
    newnode->otherbits = bits;
    newnode->child[dir] = p;
    newnode->child[1 - dir] = x;
    
    return ((char *)newhead + 1);
}

int critbit_insert(critbit_root *root, const char *key, const void* value) {
    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    void *newhead = cow_loop_insert(root, bytes, len, value);
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
