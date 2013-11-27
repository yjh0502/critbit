
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

#define MEASURE(obj, func, iter) measure(obj, #func, func, iter)
static void measure(void *obj, const char *name, measure_func func, int iter) {
    int64_t start = current_usec();

    func(obj, iter);

    int64_t diff = current_usec() - start;
    float us_per_iter = (float)diff / iter;

    printf("%s %d %.02f %.02f\n",
            name, iter, diff / 1e6, us_per_iter * 1e3);
}

void fill_rand(char *out, int len) {
    int i;
    for(i = 0; i < len-1; i++) {
        out[i] = (rand() % 0x5e) + 0x20;
    }
    out[len] = '\0';
}

void set_rand(void *obj, int iter) {
    srand(0);
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
    srand(0);
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
            /*
            printf("Failed to get `%s`\n", buf);
            exit(-1);
            */
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
            /*
            printf("Failed to get `%s`\n", buf);
            exit(-1);
            */
        }
    }
}

void cleanup(void *obj, int iter) {
    clear(obj);
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

    if(opt_int("RAND", 0)) {
        MEASURE(obj, set_rand, iter);
        MEASURE(obj, get_rand, iter);
        MEASURE(obj, cleanup, iter);
    } else {
        MEASURE(obj, set, iter);
        MEASURE(obj, get, iter);
        MEASURE(obj, cleanup, iter);
    }

    return 0;
}
