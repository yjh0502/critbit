
#include "helper.h"

#include <stdio.h>

void set(int iter) {
    int i;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);
    }
}

void get(int iter) {
    int i;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);
    }
}

void cleanup(int iter) {
}

int main(void) {
    measure(set, iter);
    measure(get, iter);
    return 0;
}
