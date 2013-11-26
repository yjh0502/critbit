
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

static int64_t current_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

void measure(void (*func)(int), int iter) {
    int64_t start = current_usec();

    func(iter);

    int64_t diff = current_usec() - start;
    float us_per_iter = (float)diff / iter;

    printf("time: %d iters, %.02f sec (%.02f ns per op)\n",
            iter, diff / 1e6, us_per_iter * 1e3);
}
