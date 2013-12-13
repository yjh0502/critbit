// Critical-bit tree implementation
// TODO: assume sizeof(log) == sizeof(node)

#include "queue.h"
#include <stdio.h>
#include <stdlib.h>

#define CRITBIT_LOG_INSERT 1
#define CRITBIT_LOG_DELETE 2

typedef struct critbit_log {
    STAILQ_ENTRY(critbit_log) entry;
    void *head;
    int op;
    int readers;
} critbit_log;

typedef struct critbit_node critbit_node;
typedef struct critbit_root {
    critbit_log *loghead;
    critbit_node *freelist;
    pthread_rwlock_t lock;
    STAILQ_HEAD(loghead, critbit_log) logs;
} critbit_root;

#include "critbit_common.h"

critbit_log *critbit_log_new(critbit_root *root, void *head, int op) {
    critbit_log *log = alloc_node(root);
    if(!log)
        return NULL;

    log->head = head;
    log->op = op;
    log->readers = 0;
    STAILQ_INSERT_TAIL(&root->logs, log, entry);
    return log;
}

critbit_root *critbit_new(void) {
    if(sizeof(critbit_log) != sizeof(critbit_node)) {
        fprintf(stderr, "sizeof(critbit_log) should same with sizeof(critbit_node)");
        exit(-1);
    }

    critbit_root *root = malloc(sizeof(critbit_root));
    root->loghead = NULL;
    root->freelist = NULL;
    pthread_rwlock_init(&root->lock, NULL);
    STAILQ_INIT(&root->logs);

    return root;
}

static void* cow_stack_insert(critbit_root *root, void *head,
        const uint8_t *bytes, const size_t bytelen, const void *value) {
    if(!head)
        return alloc_string((char *)bytes, bytelen, value);

    uint8_t *p = find_nearest(head, bytes, bytelen);
    
    uint32_t newbyte, newotherbits;
    for(newbyte = 0; newbyte < bytelen; ++newbyte) {
        if(p[newbyte] != bytes[newbyte]) {
            newotherbits = p[newbyte] ^ bytes[newbyte];
            goto found;
        }
    }
    if(p[newbyte]) {
        newotherbits = p[newbyte];
        goto found;
    }
    return NULL;

found:
    while(newotherbits & (newotherbits - 1)) {
        newotherbits &= newotherbits - 1;
    }
    newotherbits ^= 0xFF;
    uint8_t c = p[newbyte];
    int newdirection = (1 + (newotherbits | c)) >> 8;

    critbit_node *newhead = alloc_node(root);
    critbit_node *node = newhead;
    while(IS_INTERNAL(head)) {
        struct critbit_node *q = TO_NODE(head);
        if(q->byte > newbyte)
            break;
        if(q->byte == newbyte && q->otherbits > newotherbits)
            break;

        int dir = get_direction(q, bytes, bytelen);
        head = q->child[dir];

        memcpy(node, q, sizeof(critbit_node));
        critbit_node *nextnode = alloc_node(root);
        node->child[dir] = TO_NODE(nextnode);
        node = nextnode;
    }

    node->byte = newbyte;
    node->otherbits = newotherbits;
    node->child[1 - newdirection] = alloc_string((char *)bytes, bytelen, value);
    node->child[newdirection] = head;

    return TO_NODE(newhead);
}

static void cow_insert_cleanup(critbit_root *root, critbit_node *oldhead, critbit_node *newhead) {
    if(!oldhead)
        return;

    critbit_node *oldnode = NULL, *newnode = NULL;
    while(IS_INTERNAL(oldhead) && IS_INTERNAL(newhead)) {
        oldnode = TO_NODE(oldhead);
        newnode = TO_NODE(newhead);

        if(oldnode->child[0] == newnode->child[0]) {
            oldhead = oldnode->child[1];
            newhead = newnode->child[1];
            free_node(root, oldnode);
        } else if(oldnode->child[1] == newnode->child[1]) {
            oldhead = oldnode->child[0];
            newhead = newnode->child[0];
            free_node(root, oldnode);
        } else {
            break;
        }
    }
}

#define STATUS_NORMAL   0
#define STATUS_FOUND    1
#define STATUS_NOTFOUND 2

static void* cow_stack_delete(critbit_root *root, void *p,
        const uint8_t *bytes, const size_t bytelen, int *status_out) {
    int status = STATUS_NORMAL;
    critbit_node *node, *newnode;

    if(IS_INTERNAL(p)) {
        node = TO_NODE(p);
        int dir = get_direction(node, bytes, bytelen);
        critbit_node *child = cow_stack_delete(root, node->child[dir], bytes, bytelen, &status);
        switch(status) {
        case STATUS_NORMAL:
            newnode = alloc_node(root);
            memcpy(newnode, node, sizeof(critbit_node));
            newnode->child[dir] = child;

            return ((char *)newnode) + 1;
        case STATUS_FOUND:
            *status_out = STATUS_NORMAL;
            return node->child[1-dir];
        case STATUS_NOTFOUND:
            *status_out = STATUS_NOTFOUND;
            return NULL;
        }
    }

    if(strcmp((const char *)p, (const char *)bytes)) {
        *status_out = STATUS_NOTFOUND;
    } else {
        *status_out = STATUS_FOUND;
    }

    return NULL;
}

static void cow_delete_cleanup(critbit_root *root, void *oldhead, void *newhead) {
    critbit_node *oldnode = NULL, *newnode = NULL;
    while(IS_INTERNAL(newhead)) {
        oldnode = TO_NODE(oldhead);
        newnode = TO_NODE(newhead);

        if(oldnode->child[0] == newnode->child[0]) {
            oldhead = oldnode->child[1];
            newhead = newnode->child[1];
            free_node(root, oldnode);
        } else if(oldnode->child[1] == newnode->child[1]) {
            oldhead = oldnode->child[0];
            newhead = newnode->child[0];
            free_node(root, oldnode);
        } else {
            break;
        }
    }

    if(IS_INTERNAL(oldhead)) {
        oldnode = TO_NODE(oldhead);
        int dir = oldnode->child[0] == newhead;
        free_string(oldnode->child[dir]);
        free_node(root, oldnode);
    } else {
        free_string(oldhead);
    }
}

// Runs in critical section
#define MAX_READERS (1<<30)
static void collect_garbage(critbit_root *root) {
    critbit_log *prevlog = STAILQ_FIRST(&root->logs);
    critbit_log *nextlog = STAILQ_NEXT(prevlog, entry);
    while(nextlog) {
        if(prevlog->readers >= 0) {
            __sync_fetch_and_sub(&prevlog->readers, MAX_READERS);
        }
        if(prevlog->readers != -MAX_READERS) {
            return;
        }

        switch(nextlog->op) {
        case CRITBIT_LOG_INSERT:
            cow_insert_cleanup(root, prevlog->head, nextlog->head);
            break;
        case CRITBIT_LOG_DELETE:
            cow_delete_cleanup(root, prevlog->head, nextlog->head);
            break;
        }
        STAILQ_REMOVE_HEAD(&root->logs, entry);
        free_node(root, prevlog);

        prevlog = nextlog;
        nextlog = STAILQ_NEXT(nextlog, entry);
    }
}

int critbit_insert(critbit_root *root, const char *key, const void* value) {
    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);

    pthread_rwlock_wrlock(&root->lock);

    int ret = 0;
    void *head = NULL;
    if(root->loghead)
        head = root->loghead->head;

    void *newhead = cow_stack_insert(root, head, bytes, len, value);
    if(newhead) {
        // sizeof(critbit_log) == sizeof(critbit_node)
        critbit_log *log = critbit_log_new(root, newhead, CRITBIT_LOG_INSERT);
        if(log) {
            root->loghead = log;
        } else {
            ret = 1;
        }
    } else {
        ret = 1;
    }
    collect_garbage(root);

    pthread_rwlock_unlock(&root->lock);

    return ret;
}

int critbit_delete(critbit_root *root, const char *key) {
    if(!root->loghead)
        return 1;

    pthread_rwlock_wrlock(&root->lock);

    int status = STATUS_NORMAL;
    const uint8_t *bytes = (void *)key;
    const size_t len = strlen(key);
    critbit_node *newhead = cow_stack_delete(root, root->loghead->head,
            bytes, len, &status);

    int ret = 0;
    if(!newhead && status == STATUS_NOTFOUND) {
        ret = 1;
    } else {
        critbit_log *log = critbit_log_new(root, newhead, CRITBIT_LOG_DELETE);
        if(log) {
            root->loghead = log;
        } else {
            ret = 1;
        }
    }
    collect_garbage(root);

    pthread_rwlock_unlock(&root->lock);

    return ret;
}

static critbit_log *get_head(critbit_root *root) {
    for(;;) {
        critbit_log *log = root->loghead;
        // log->readers == 0: No reader
        // log->readers > 0: There are active readers, not marked for GC
        // log->readers < 0: Marked for GC, wait readers to exit
        if(__sync_add_and_fetch(&log->readers, 1) >= 0) {
            return log;
        }
    }
}
static void release_head(critbit_root *root, critbit_log *log) {
    __sync_fetch_and_sub(&log->readers, 1);
}

int critbit_get(critbit_root *root, const char *key, void **out) {
    const int len = strlen(key);
    const uint8_t *bytes = (void *)key;

    critbit_log *log = get_head(root);

    char *nearest = find_nearest(log->head, bytes, len);
    int ret = 1;
    if(strcmp(nearest, key) == 0) {
        nearest -= sizeof(void *);
        *out = *((void **)nearest);
        ret = 0;
    }

    release_head(root, log);

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
    if(root->loghead && root->loghead->head) {
        clear_node(root->loghead->head);
    }
    free(root->loghead);

    while(root->freelist) {
        void *node = root->freelist;
        root->freelist = root->freelist->child[0];
        free(node);
    }
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
