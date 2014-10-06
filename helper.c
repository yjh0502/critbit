
#include "helper.h"

#include <sys/time.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int64_t current_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

typedef void (*measure_func)(void*, int);

typedef struct stat {
    const char *name;
    int iter;
    int usec;
} stat;

#define MEASURE(obj, func, iter) measure(obj, #func, func, iter)
static stat measure(void *obj, const char *name, measure_func func, int iter) {
    int64_t start = current_usec();

    func(obj, iter);

    int64_t diff = current_usec() - start;

    stat s = {name, iter, diff};
    return s;
}

void fill_rand(char *out, int len) {
    int i;
    for(i = 0; i < len; i++) {
        out[i] = (rand() & 0xFF) | 0x80;
    }
    out[len] = '\0';
}

#define RANDOM_SEED 10
void set_rand(void *obj, int iter) {
    srand(RANDOM_SEED);
    int i;

    char buf[20];
    for(i = 1; i < iter; ++i) {
        fill_rand(buf, 10);
        void* val = (void *)(size_t)i;

        if(add(obj, (void *)&buf, val)) {
            printf("Failed to insert `%s`\n", buf);
            exit(-1);
        }
    }
}

void set(void *obj, int iter) {
    int i;
    char buf[20];
    for(i = 1; i < iter; ++i) {
        sprintf(buf, "%09d", i);
        void* val = (void *)(size_t)i;

        if(add(obj, buf, val)) {
            printf("Failed to insert `%s`\n", buf);
            exit(-1);
        }
    }
}

void get_rand(void *obj, int iter) {
    srand(RANDOM_SEED);
    int i;

    char buf[20];
    for(i = 1; i < iter; ++i) {
        fill_rand(buf, 10);
        void *out = find(obj, buf);
        if(out) {
            int val = (size_t)out;
            if(val != i) {
                printf("invalid value: %d != %d\n", i, val);
                exit(-1);
            }
        } else {
            printf("Failed to get `%s`\n", buf);
            exit(-1);
        }
    }

}

void get_rand_100(void *obj, int iter) {
    const int P = 100;
    int i, j;
    iter /= P;
    for(j = 0; j < P; j++) {
        srand(RANDOM_SEED);

        char buf[20];
        for(i = 1; i < iter; ++i) {
            fill_rand(buf, 10);
            void *out = find(obj, buf);
            if(out) {
                int val = (size_t)out;
                if(val != i) {
                    printf("invalid value: %d != %d\n", i, val);
                    exit(-1);
                }
            } else {
                printf("Failed to get `%s`\n", buf);
                exit(-1);
            }
        }
    }
}


void get(void *obj, int iter) {
    int i;
    char buf[20];
    for(i = 1; i < iter; ++i) {
        sprintf(buf, "%09d", i);
        void *out = find(obj, buf);
        if(out) {
            int val = (size_t)out;
            if(val != i) {
                printf("invalid value: %d != %d\n", i, val);
                exit(-1);
            }
        } else {
            printf("Failed to get `%s`\n", buf);
            exit(-1);
        }
    }
}

#include <pthread.h>
#define NUM_THREADS 32
int thread_iter;
void *get_thread(void *obj) {
    int i;
    char buf[20];
    for(i = 1; i < thread_iter; ++i) {
        int n = rand() % thread_iter;
        sprintf(buf, "%09d", n);
        void* val = (void *)(size_t)n;
        if(i % 200 == 0) {
            del(obj, buf);
            add(obj, buf, val);
        } else {
            find(obj, buf);
        }
    }
    return NULL;
}
void get_threaded(void *obj, int iter) {
    pthread_t threads[NUM_THREADS];
    int i;
    thread_iter = iter;
    for(i = 0; i < NUM_THREADS; i++) {
        pthread_create(threads + i, NULL, get_thread, obj);
    }
    for(i = 0; i < NUM_THREADS; i++) {
        void *out;
        pthread_join(threads[i], &out);
    }
}

void cleanup_rand(void *obj, int iter) {
    srand(RANDOM_SEED);
    int i;
    char buf[20];
    for(i = 1; i < iter; ++i) {
        fill_rand(buf, 10);
        if(del(obj, buf)) {
            printf("Failed to delete: %s\n", buf);
        }
    }
}

void cleanup(void *obj, int iter) {
    int i;
    char buf[20];
    for(i = 1; i < iter; ++i) {
        sprintf(buf, "%09d", i);
        if(del(obj, buf)) {
            printf("Failed to delete: %s\n", buf);
            exit(-1);
        }
    }
}

static const char *opt_str(const char *name) {
    return getenv(name);
}
static int opt_int(const char *name, int fallback) {
    const char *env = opt_str(name);
    if(!env)
        return fallback;
    int val = atoi(env);
    if(!val)
        return fallback;
    return val;
}

int main(void) {
    int iter = opt_int("ITER", 10000);
    void *obj = init();

    int size = 0;
    stat stats[20];

    stats[size++] = MEASURE(obj, set_rand, iter);
    stats[size++] = MEASURE(obj, get_rand, iter);
    stats[size++] = MEASURE(obj, cleanup_rand, iter);

    /*
    stats[size++] = MEASURE(obj, set, iter);
    stats[size++] = MEASURE(obj, get, iter);
    stats[size++] = MEASURE(obj, cleanup, iter);
    */

    stats[size++] = MEASURE(obj, set, iter);
    stats[size++] = MEASURE(obj, get_threaded, iter);
    stats[size-1].iter *= NUM_THREADS;
    stats[size++] = MEASURE(obj, cleanup, iter);

    int i;
    printf("%.2f\t", iter / 1e6);
    for(i = 0; i < size; i++) {
        printf("%.2f\t", (stats[i].usec * 1e3f / stats[i].iter));
    }


    clear(obj);

    return 0;
}
