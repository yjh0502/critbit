// Critical-bit tree implementation

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define NODE_CACHE 0
#include "critbit_common.h"

int critbit_insert(critbit_root *root, const char *key, const void* value) {
    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    uint8_t *p = root->head;
    if(!p) {
        p = alloc_string(key, len, value);
        if(!p)
            return 1;

        root->head = p;
        return 0;
    }

    p = find_nearest(root->head, bytes, len);
    
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

    struct critbit_node *node = alloc_aligned(sizeof(struct critbit_node));
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

    void **wherep = &root->head;
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
