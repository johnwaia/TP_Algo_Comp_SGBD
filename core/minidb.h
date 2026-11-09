#ifndef MINIDB_H
#define MINIDB_H

#include <stddef.h>
#include <stdint.h>

#define MDB_MAX_COLS     16
#define MDB_MAX_NAME     32
#define MDB_MAX_TABLES   16
#define MDB_MAX_CONDS    16
#define MDB_TEXT_DEFAULT 32

typedef enum { TYPE_INT, TYPE_TEXT } ColType;

typedef enum { INDEX_NONE, INDEX_HASH, INDEX_AVL, INDEX_LIST } IndexKind;

typedef enum { GROWTH_DOUBLING, GROWTH_LINEAR } GrowthPolicy;

typedef struct {
    ColType type;
    int64_t i;
    const char *s;
} Value;

#define VAL_INT(x)  ((Value){ TYPE_INT, (int64_t)(x), NULL })
#define VAL_TEXT(x) ((Value){ TYPE_TEXT, 0, (x) })

typedef struct {
    char name[MDB_MAX_NAME];
    ColType type;
    size_t size;
    size_t offset;
} Column;

typedef struct Index Index;

typedef struct {
    char name[MDB_MAX_NAME];
    Column cols[MDB_MAX_COLS];
    int ncols;
    size_t row_size;
    unsigned char *rows;
    size_t nrows;
    size_t capacity;
    GrowthPolicy growth;
    size_t realloc_count;
    Index *indexes[MDB_MAX_COLS];
} Table;

typedef struct {
    Table *tables[MDB_MAX_TABLES];
    int ntables;
} Database;

typedef enum { OP_EQ, OP_LT, OP_LE, OP_GT, OP_GE, OP_BETWEEN } Op;

typedef struct {
    const char *column;
    Op op;
    Value v1;
    Value v2;
} Condition;

typedef struct {
    size_t *ids;
    size_t count;
    size_t capacity;
    const char *plan;
} ResultSet;

Database *db_create(void);
void db_free(Database *db);
Table *db_get_table(Database *db, const char *name);

Table *create_table_struct(Database *db, const char *definition);
void table_set_growth(Table *t, GrowthPolicy policy);
int column_index(const Table *t, const char *name);

int insert_row(Table *t, const Value values[], int n);
Value get_value(const Table *t, size_t row, int col);

int create_index(Table *t, const char *column, IndexKind kind);

ResultSet select_values(Table *t, const Condition conds[], int n);
ResultSet select_scan(Table *t, const Condition conds[], int n);
void result_free(ResultSet *rs);

void print_rows(const Table *t, const ResultSet *rs, const char *cols[], int ncols);
const char *index_kind_name(IndexKind kind);

#endif
