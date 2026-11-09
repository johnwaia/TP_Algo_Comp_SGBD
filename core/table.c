#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index.h"

static void index_free(Index *ix) {
    if (ix->kind == INDEX_HASH) hash_free(ix->hash);
    else if (ix->kind == INDEX_AVL) avl_free(ix->avl);
    else list_free(ix->list);
    free(ix);
}

static void index_insert(Index *ix, Value key, size_t id) {
    if (ix->kind == INDEX_HASH) hash_insert(ix->hash, key, id);
    else if (ix->kind == INDEX_AVL) ix->avl = avl_insert(ix->avl, key, id);
    else ix->list = list_insert(ix->list, key, id);
}

Database *db_create(void) {
    return calloc(1, sizeof(Database));
}

void db_free(Database *db) {
    if (db == NULL) return;
    for (int i = 0; i < db->ntables; i++) {
        Table *t = db->tables[i];
        for (int c = 0; c < t->ncols; c++) {
            if (t->indexes[c] != NULL) index_free(t->indexes[c]);
        }
        free(t->rows);
        free(t);
    }
    free(db);
}

Table *db_get_table(Database *db, const char *name) {
    for (int i = 0; i < db->ntables; i++) {
        if (strcmp(db->tables[i]->name, name) == 0) return db->tables[i];
    }
    return NULL;
}

int column_index(const Table *t, const char *name) {
    for (int c = 0; c < t->ncols; c++) {
        if (strcmp(t->cols[c].name, name) == 0) return c;
    }
    return -1;
}

const char *index_kind_name(IndexKind kind) {
    if (kind == INDEX_HASH) return "index hash";
    if (kind == INDEX_AVL) return "index AVL";
    if (kind == INDEX_LIST) return "index liste triee";
    return "aucun index";
}

static void skip_spaces(const char **p) {
    while (isspace((unsigned char)**p)) (*p)++;
}

static int read_word(const char **p, char *out, int max) {
    int n = 0;
    skip_spaces(p);
    while ((isalnum((unsigned char)**p) || **p == '_') && n < max - 1) {
        out[n] = **p;
        n++;
        (*p)++;
    }
    out[n] = '\0';
    return n > 0;
}

static int parse_column(const char **p, Table *t) {
    Column *c = &t->cols[t->ncols];
    char type[16];

    if (!read_word(p, c->name, MDB_MAX_NAME)) return 0;
    if (column_index(t, c->name) >= 0) return 0;
    if (!read_word(p, type, sizeof(type))) return 0;

    if (strcmp(type, "INT") == 0) {
        c->type = TYPE_INT;
        c->size = sizeof(int64_t);
    } else if (strcmp(type, "TEXT") == 0) {
        long len = MDB_TEXT_DEFAULT;
        skip_spaces(p);
        if (**p == '(') {
            char *end;
            len = strtol(*p + 1, &end, 10);
            *p = end;
            skip_spaces(p);
            if (len <= 0 || len > 4096 || **p != ')') return 0;
            (*p)++;
        }
        c->type = TYPE_TEXT;
        c->size = len + 1;
    } else {
        return 0;
    }

    c->offset = t->row_size;
    t->row_size += c->size;
    t->ncols++;
    return 1;
}

static int parse_definition(Database *db, Table *t, const char *p) {
    if (!read_word(&p, t->name, MDB_MAX_NAME)) return 0;
    if (db_get_table(db, t->name) != NULL) return 0;

    skip_spaces(&p);
    if (*p != '(') return 0;
    p++;

    while (1) {
        if (t->ncols >= MDB_MAX_COLS) return 0;
        if (!parse_column(&p, t)) return 0;
        skip_spaces(&p);
        if (*p == ')') break;
        if (*p != ',') return 0;
        p++;
    }
    p++;

    skip_spaces(&p);
    return *p == '\0';
}

Table *create_table_struct(Database *db, const char *def) {
    if (db == NULL || def == NULL || db->ntables >= MDB_MAX_TABLES) return NULL;

    Table *t = calloc(1, sizeof(Table));
    if (!parse_definition(db, t, def)) {
        free(t);
        return NULL;
    }

    t->growth = GROWTH_DOUBLING;
    db->tables[db->ntables] = t;
    db->ntables++;
    return t;
}

void table_set_growth(Table *t, GrowthPolicy policy) {
    t->growth = policy;
}

static void table_grow(Table *t) {
    if (t->growth == GROWTH_DOUBLING) {
        t->capacity = t->capacity == 0 ? 16 : t->capacity * 2;
    } else {
        t->capacity = t->capacity + 16;
    }
    t->rows = realloc(t->rows, t->capacity * t->row_size);
    t->realloc_count++;
}

Value get_value(const Table *t, size_t row, int col) {
    unsigned char *cell = t->rows + row * t->row_size + t->cols[col].offset;
    if (t->cols[col].type == TYPE_INT) {
        int64_t v;
        memcpy(&v, cell, sizeof(v));
        return VAL_INT(v);
    }
    return VAL_TEXT((char *)cell);
}

int insert_row(Table *t, const Value values[], int n) {
    if (t == NULL || n != t->ncols) return -1;

    for (int c = 0; c < n; c++) {
        if (values[c].type != t->cols[c].type) return -1;
        if (values[c].type == TYPE_TEXT) {
            if (values[c].s == NULL) return -1;
            if (strlen(values[c].s) + 1 > t->cols[c].size) return -1;
        }
    }

    if (t->nrows == t->capacity) table_grow(t);

    unsigned char *row = t->rows + t->nrows * t->row_size;
    memset(row, 0, t->row_size);
    for (int c = 0; c < n; c++) {
        unsigned char *cell = row + t->cols[c].offset;
        if (values[c].type == TYPE_INT) {
            memcpy(cell, &values[c].i, sizeof(int64_t));
        } else {
            strcpy((char *)cell, values[c].s);
        }
    }

    size_t id = t->nrows;
    t->nrows++;

    for (int c = 0; c < n; c++) {
        if (t->indexes[c] != NULL) index_insert(t->indexes[c], values[c], id);
    }
    return 0;
}

int create_index(Table *t, const char *column, IndexKind kind) {
    int col = column_index(t, column);
    if (col < 0 || t->indexes[col] != NULL) return -1;
    if (kind != INDEX_HASH && kind != INDEX_AVL && kind != INDEX_LIST) return -1;

    Index *ix = calloc(1, sizeof(Index));
    ix->kind = kind;
    if (kind == INDEX_HASH) ix->hash = hash_new();

    for (size_t r = 0; r < t->nrows; r++) {
        index_insert(ix, get_value(t, r, col), r);
    }
    t->indexes[col] = ix;
    return 0;
}

static int check_conditions(const Table *t, const Condition conds[], int n, int cols[]) {
    if (t == NULL || n < 0 || n > MDB_MAX_CONDS) return 0;

    for (int i = 0; i < n; i++) {
        int col = column_index(t, conds[i].column);
        if (col < 0) return 0;

        ColType type = t->cols[col].type;
        if (conds[i].v1.type != type) return 0;
        if (type == TYPE_TEXT && conds[i].v1.s == NULL) return 0;
        if (conds[i].op == OP_BETWEEN) {
            if (conds[i].v2.type != type) return 0;
            if (type == TYPE_TEXT && conds[i].v2.s == NULL) return 0;
        }
        cols[i] = col;
    }
    return 1;
}

static int row_matches(const Table *t, const Condition conds[], const int cols[], int n, size_t row) {
    for (int i = 0; i < n; i++) {
        if (!value_match(get_value(t, row, cols[i]), &conds[i])) return 0;
    }
    return 1;
}

ResultSet select_scan(Table *t, const Condition conds[], int n) {
    ResultSet rs = {0};
    int cols[MDB_MAX_CONDS];

    if (!check_conditions(t, conds, n, cols)) {
        rs.plan = "erreur : condition invalide";
        return rs;
    }

    for (size_t r = 0; r < t->nrows; r++) {
        if (row_matches(t, conds, cols, n, r)) result_push(&rs, r);
    }
    rs.plan = "parcours complet";
    return rs;
}

static int index_score(Index *ix, Op op) {
    if (ix == NULL) return 0;
    if (op == OP_EQ) {
        if (ix->kind == INDEX_HASH) return 3;
        if (ix->kind == INDEX_AVL) return 2;
        return 1;
    }
    if (ix->kind == INDEX_AVL) return 2;
    if (ix->kind == INDEX_LIST) return 1;
    return 0;
}

ResultSet select_values(Table *t, const Condition conds[], int n) {
    ResultSet rs = {0};
    int cols[MDB_MAX_CONDS];

    if (!check_conditions(t, conds, n, cols)) {
        rs.plan = "erreur : condition invalide";
        return rs;
    }

    int best = -1;
    int best_score = 0;
    for (int i = 0; i < n; i++) {
        int s = index_score(t->indexes[cols[i]], conds[i].op);
        if (s > best_score) {
            best_score = s;
            best = i;
        }
    }

    if (best == -1) return select_scan(t, conds, n);

    Index *ix = t->indexes[cols[best]];
    const Condition *c = &conds[best];
    ResultSet candidates = {0};

    if (c->op == OP_EQ) {
        if (ix->kind == INDEX_HASH) hash_find(ix->hash, c->v1, &candidates);
        else if (ix->kind == INDEX_AVL) avl_find(ix->avl, c->v1, &candidates);
        else list_find(ix->list, c->v1, &candidates);
    } else {
        if (ix->kind == INDEX_AVL) avl_find_range(ix->avl, c, &candidates);
        else list_find_range(ix->list, c, &candidates);
    }

    for (size_t k = 0; k < candidates.count; k++) {
        if (row_matches(t, conds, cols, n, candidates.ids[k])) {
            result_push(&rs, candidates.ids[k]);
        }
    }
    result_free(&candidates);

    rs.plan = index_kind_name(ix->kind);
    return rs;
}

void print_rows(const Table *t, const ResultSet *rs, const char *cols[], int ncols) {
    int idx[MDB_MAX_COLS];
    int n = 0;

    if (cols == NULL) {
        for (int c = 0; c < t->ncols; c++) {
            idx[n] = c;
            n++;
        }
    } else {
        for (int i = 0; i < ncols && n < MDB_MAX_COLS; i++) {
            int c = column_index(t, cols[i]);
            if (c >= 0) {
                idx[n] = c;
                n++;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        printf("%-14s", t->cols[idx[i]].name);
    }
    printf("\n");

    for (size_t k = 0; k < rs->count; k++) {
        for (int i = 0; i < n; i++) {
            Value v = get_value(t, rs->ids[k], idx[i]);
            if (v.type == TYPE_INT) printf("%-14lld", (long long)v.i);
            else printf("%-14s", v.s);
        }
        printf("\n");
    }

    printf("(%zu ligne(s), plan : %s)\n\n", rs->count, rs->plan ? rs->plan : "-");
}
