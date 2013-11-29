// Critical-bit tree implementation

#define NODE_CACHE 1
#include "critbit_common.h"

static void* cow_stack_insert(critbit_root *root, void *p,
        const uint8_t *bytes, const size_t bytelen, const void *value) {
    critbit_node *node;
    if(!p) {
        return alloc_string((char *)bytes, bytelen, value);
    }

    if(IS_INTERNAL(p)) {
        node = TO_NODE(p);
        int dir = get_direction(node, bytes, bytelen);
        critbit_node *child = cow_stack_insert(root, node->child[dir], bytes, bytelen, value);
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

    void *newhead = cow_stack_insert(root, root->head, bytes, len, value);
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
