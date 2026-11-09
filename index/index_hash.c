#include <stdlib.h>

#include "index.h"

typedef struct HashEntry {
    Value key;
    ResultSet ids;
    struct HashEntry *next;
} HashEntry;

struct HashTable {
    HashEntry **buckets;
    size_t size;
    size_t count;
};

static size_t hash(Value v, size_t size) {
    size_t h = 0;
    if (v.type == TYPE_INT) {
        h = (size_t)v.i;
    } else {
        for (const char *p = v.s; *p != '\0'; p++) {
            h = h * 31 + (unsigned char)*p;
        }
    }
    return h % size;
}

HashTable *hash_new(void) {
    HashTable *h = malloc(sizeof(HashTable));
    h->size = 64;
    h->count = 0;
    h->buckets = calloc(h->size, sizeof(HashEntry *));
    return h;
}

static void hash_grow(HashTable *h) {
    size_t new_size = h->size * 2;
    HashEntry **new_buckets = calloc(new_size, sizeof(HashEntry *));

    for (size_t i = 0; i < h->size; i++) {
        HashEntry *e = h->buckets[i];
        while (e != NULL) {
            HashEntry *next = e->next;
            size_t j = hash(e->key, new_size);
            e->next = new_buckets[j];
            new_buckets[j] = e;
            e = next;
        }
    }

    free(h->buckets);
    h->buckets = new_buckets;
    h->size = new_size;
}

static HashEntry *hash_lookup(HashTable *h, Value key) {
    HashEntry *e = h->buckets[hash(key, h->size)];
    while (e != NULL && value_cmp(e->key, key) != 0) {
        e = e->next;
    }
    return e;
}

void hash_insert(HashTable *h, Value key, size_t id) {
    HashEntry *e = hash_lookup(h, key);
    if (e == NULL) {
        if (h->count >= h->size) hash_grow(h);
        size_t i = hash(key, h->size);
        e = calloc(1, sizeof(HashEntry));
        e->key = value_copy(key);
        e->next = h->buckets[i];
        h->buckets[i] = e;
        h->count++;
    }
    result_push(&e->ids, id);
}

void hash_find(HashTable *h, Value key, ResultSet *out) {
    HashEntry *e = hash_lookup(h, key);
    if (e != NULL) result_push_all(out, &e->ids);
}

void hash_free(HashTable *h) {
    for (size_t i = 0; i < h->size; i++) {
        HashEntry *e = h->buckets[i];
        while (e != NULL) {
            HashEntry *next = e->next;
            value_free(e->key);
            result_free(&e->ids);
            free(e);
            e = next;
        }
    }
    free(h->buckets);
    free(h);
}
