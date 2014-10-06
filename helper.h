
void* init(void);

// adds new one. return 0 if success, 1 if failed (already exists)
int add(void *obj, const char *key, void *val);
// returns NULL if not exists
void* find(void *obj, const char *key);
// returns 0 if success, 1 if failed (not exists)
int del(void *obj, const char *key);
// removes all elements
void clear(void *obj);

// prints datastructure-specific stats
void info(void *obj);
