#include <stdlib.h>
#include <string.h>

#include "index.h"

int value_cmp(Value a, Value b) {
    if (a.type == TYPE_INT) {
        if (a.i < b.i) return -1;
        if (a.i > b.i) return 1;
        return 0;
    }
    return strcmp(a.s, b.s);
}

Value value_copy(Value v) {
    if (v.type == TYPE_TEXT) {
        char *s = malloc(strlen(v.s) + 1);
        strcpy(s, v.s);
        v.s = s;
    }
    return v;
}

void value_free(Value v) {
    if (v.type == TYPE_TEXT) free((char *)v.s);
}

int value_match(Value v, const Condition *c) {
    int a = value_cmp(v, c->v1);
    switch (c->op) {
        case OP_EQ: return a == 0;
        case OP_LT: return a < 0;
        case OP_LE: return a <= 0;
        case OP_GT: return a > 0;
        case OP_GE: return a >= 0;
        case OP_BETWEEN: return a >= 0 && value_cmp(v, c->v2) <= 0;
    }
    return 0;
}

void result_push(ResultSet *rs, size_t id) {
    if (rs->count == rs->capacity) {
        rs->capacity = rs->capacity == 0 ? 8 : rs->capacity * 2;
        rs->ids = realloc(rs->ids, rs->capacity * sizeof(size_t));
    }
    rs->ids[rs->count] = id;
    rs->count++;
}

void result_push_all(ResultSet *rs, const ResultSet *src) {
    for (size_t i = 0; i < src->count; i++) {
        result_push(rs, src->ids[i]);
    }
}

void result_free(ResultSet *rs) {
    free(rs->ids);
    rs->ids = NULL;
    rs->count = 0;
    rs->capacity = 0;
}
