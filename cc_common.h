void* init(void) {
    MAP_TYPE *m = new MAP_TYPE();
    return m;
}

int add(void *obj, const char *key, void *val) {
    MAP_TYPE *m = (MAP_TYPE *)obj;
    auto iter = m->insert(std::pair<std::string, void *>(std::string(key), val));
    return !iter.second;
}

void* find(void *obj, const char *key) {
    MAP_TYPE *m = (MAP_TYPE *)obj;
    auto iter = m->find(std::string(key));
    return iter->second;
}

int del(void *obj, const char *key) {
    MAP_TYPE *m = (MAP_TYPE *)obj;
    return !m->erase(std::string(key));
}

void clear(void *obj) {
    MAP_TYPE *m = (MAP_TYPE *)obj;
    m->clear();
}
