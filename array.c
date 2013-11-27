
#include "helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char ** array;

void set(int iter) {
    int i;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);

        if(!array[i]) {
            int len = strlen(buf);
            char *str = malloc(len + 1);
            memcpy(str, buf, len + 1);
            array[i] = str;
        }
    }
}

void get(int iter) {
    int i;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);

        if(strcmp(array[i], buf) != 0) {
            printf("invalid value: %s != %s\n", array[i], buf);
        }
    }
}

void cleanup(int iter) {
    int i;
    for(i = 0; i < iter; i++) {
        free(array[i]);
    }
    free(array);
}

int main(void) {
    int size = iter * sizeof(char *);
    array = malloc(size);
    memset(array, 0, size);

    measure(set, iter);
    measure(get, iter);
    measure(cleanup, iter);

    return 0;
}
