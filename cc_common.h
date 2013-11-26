void set(int iter) {
    int i;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);

        m[std::string(buf)] = i;
    }
}

void get(int iter) {
    int i;
    char buf[20];
    for(i = 0; i < iter; i++) {
        sprintf(buf, "%09d", i);
        int val = m[std::string(buf)];
        if(i != val) {
            printf("invalid value: %d != %d\n", i, val);
            exit(-1);
        }
    }
}

void cleanup(int iter) {
    m.clear();
}

int main(void) {
    measure(set, iter);
    measure(get, iter);
    measure(cleanup, iter);

    return 0;
}
