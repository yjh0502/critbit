// Critical-bit tree implementation

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct critbit_root {
    void *head;

    pthread_mutex_t wlock;
    pthread_rwlock_t left_lock, right_lock;
    pthread_rwlock_t *cur_rwlock, *prev_rwlock;
} critbit_root;

#include "critbit_common.h"

critbit_root *critbit_new(void) {
    critbit_root *root = malloc(sizeof(critbit_root));
    root->head = NULL;

    root->wlock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    root->left_lock = (pthread_rwlock_t)PTHREAD_RWLOCK_INITIALIZER;
    root->right_lock = (pthread_rwlock_t)PTHREAD_RWLOCK_INITIALIZER;

    root->cur_rwlock = &root->left_lock;
    root->prev_rwlock = &root->right_lock;

    return root;
}

static int critbit_insert_inplace(critbit_root *root,
        const char *key, int keylen, const void* value) {
    const uint8_t *bytes = (void *)key;

    uint8_t *p = root->head;
    if(!p) {
        p = alloc_string(key, keylen, value);
        if(!p)
            return 1;

        root->head = p;
        return 0;
    }

    p = find_nearest(root->head, bytes, keylen);

    uint32_t newbyte;
    uint8_t newotherbits;
    for(newbyte = 0; newbyte < keylen; ++newbyte) {
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

    char *x = alloc_string(key, keylen, value);
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

        wherep = q->child + get_direction(q, bytes, keylen);
    }
    node->child[newdirection] = *wherep;
    __sync_synchronize();
    *wherep = FROM_NODE(node);
    __sync_synchronize();

    return 0;
}

int critbit_insert(critbit_root *root, const char *key, const void* value) {
    const size_t len = strlen(key);

    pthread_mutex_lock(&root->wlock);
    int ret = critbit_insert_inplace(root, key, len, value);
    pthread_mutex_unlock(&root->wlock);

    return ret;
}

static void * critbit_delete_inplace(critbit_root *root, const char *key, int keylen) {
    if(!root->head) return NULL;

    const uint8_t *bytes = (void *)key;
    uint8_t *p = root->head;
    void **wherep = &root->head, **whereq = 0;
    critbit_node *q = NULL;
    int dir = 0;

    while(IS_INTERNAL(p)) {
        whereq = wherep;
        q = TO_NODE(p);
        dir = get_direction(q, bytes, keylen);
        wherep = q->child + dir;
        p = *wherep;
    }

    if(strcmp(key, (const char *)p) != 0) {
        return NULL;
    }

    if(!whereq) {
        root->head = NULL;
        return p;
    }

    *whereq = q->child[1 - dir];
    __sync_synchronize();
    return FROM_NODE(q);
}

int critbit_delete(critbit_root *root, const char *key) {
    const size_t len = strlen(key);

    // begin critical section
    pthread_mutex_lock(&root->wlock);

    void * p = critbit_delete_inplace(root, key, len);

    pthread_rwlock_t *lock = root->cur_rwlock;
    pthread_rwlock_t *newlock = root->prev_rwlock;

    // swap two locks
    if(!__sync_val_compare_and_swap(&root->cur_rwlock, lock, newlock)) {
        printf("!!!\n");
    }
    if(!__sync_val_compare_and_swap(&root->prev_rwlock, newlock, lock)) {
        printf("!!!\n");
    }

    // wait until all readers are out
    pthread_rwlock_wrlock(lock);
    pthread_rwlock_unlock(lock);

    // end critical section
    pthread_mutex_unlock(&root->wlock);

    if(!p)
        return 1;

    if(!IS_INTERNAL(p)) {
        free_string(p);
    } else {
        critbit_node *node = TO_NODE(p);
        if(!IS_INTERNAL(node->child[0]) && strcmp(key, (const char *)node->child[0]) == 0) {
            free_string(node->child[0]);
            free_node(root, node);
        } else {
            free_string(node->child[1]);
            free_node(root, node);
        }
    }

    return 0;
}

int critbit_get(critbit_root *root, const char *key, void **out) {
    const int len = strlen(key);
    const uint8_t *bytes = (void *)key;

    pthread_rwlock_t *lock = root->cur_rwlock;
    pthread_rwlock_rdlock(lock);
    char *nearest = find_nearest(root->head, bytes, len);

    int ret = 1;
    if(strcmp(nearest, key) == 0) {
        nearest -= sizeof(void *);
        *out = *((void **)nearest);
        ret = 0;
    }
    pthread_rwlock_unlock(lock);

    return ret;
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
    free(root);
}
