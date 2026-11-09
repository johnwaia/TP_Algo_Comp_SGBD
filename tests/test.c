#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minidb.h"

static int echecs = 0;

static void check(int cond, const char *msg) {
    if (!cond) {
        printf("ECHEC : %s\n", msg);
        echecs++;
    }
}

static uint64_t etat = 42;
static uint64_t rnd(void) {
    etat ^= etat << 13;
    etat ^= etat >> 7;
    etat ^= etat << 17;
    return etat;
}

static int cmp_size(const void *a, const void *b) {
    size_t x = *(const size_t *)a, y = *(const size_t *)b;
    return (x > y) - (x < y);
}

static int memes_lignes(ResultSet *a, ResultSet *b) {
    if (a->count != b->count) return 0;
    qsort(a->ids, a->count, sizeof *a->ids, cmp_size);
    qsort(b->ids, b->count, sizeof *b->ids, cmp_size);
    return a->count == 0 || memcmp(a->ids, b->ids, a->count * sizeof *a->ids) == 0;
}

static void test_parser(void) {
    Database *db = db_create();
    check(create_table_struct(db, "t1(a INT, b TEXT, c TEXT(8))") != NULL, "definition valide");
    check(create_table_struct(db, "t1(a INT)") == NULL, "nom de table en double");
    check(create_table_struct(db, "t2(a INT, a INT)") == NULL, "colonne en double");
    check(create_table_struct(db, "t3(a FLOAT)") == NULL, "type inconnu");
    check(create_table_struct(db, "t4(a INT") == NULL, "parenthese manquante");
    check(create_table_struct(db, "t5(a TEXT(0))") == NULL, "taille TEXT invalide");

    Table *t = db_get_table(db, "t1");
    Value ok[] = { VAL_INT(1), VAL_TEXT("x"), VAL_TEXT("12345678") };
    Value trop_long[] = { VAL_INT(1), VAL_TEXT("x"), VAL_TEXT("123456789") };
    Value mauvais_type[] = { VAL_TEXT("1"), VAL_TEXT("x"), VAL_TEXT("y") };
    check(insert_row(t, ok, 3) == 0, "insertion valide");
    check(insert_row(t, trop_long, 3) != 0, "TEXT trop long refuse");
    check(insert_row(t, mauvais_type, 3) != 0, "mauvais type refuse");
    check(insert_row(t, ok, 2) != 0, "mauvais nombre de colonnes refuse");
    db_free(db);
}

static void test_index(IndexKind kind) {
    Database *db = db_create();
    Table *t = create_table_struct(db, "t(k INT, s TEXT(8), v INT)");
    create_index(t, "k", kind);

    char buf[16];
    for (int i = 0; i < 3000; i++) {
        snprintf(buf, sizeof buf, "s%03d", (int)(rnd() % 200));
        Value row[] = { VAL_INT((int64_t)(rnd() % 500) - 250), VAL_TEXT(buf),
                        VAL_INT((int64_t)(rnd() % 100)) };
        insert_row(t, row, 3);
    }
    create_index(t, "s", kind);

    for (int q = 0; q < 300; q++) {
        Op op = (Op)(rnd() % 6);
        int64_t a = (int64_t)(rnd() % 600) - 300, b = a + (int64_t)(rnd() % 100);
        char sa[16], sb[16];
        snprintf(sa, sizeof sa, "s%03d", (int)(rnd() % 200));
        snprintf(sb, sizeof sb, "s%03d", (int)(rnd() % 200));
        if (strcmp(sa, sb) > 0) { char tmp[16]; strcpy(tmp, sa); strcpy(sa, sb); strcpy(sb, tmp); }

        Condition conds[3] = {
            { "k", op, VAL_INT(a), VAL_INT(b) },
            { "s", (Op)(rnd() % 6), VAL_TEXT(sa), VAL_TEXT(sb) },
            { "v", OP_LT, VAL_INT((int64_t)(rnd() % 100)), {0} },
        };
        for (int n = 1; n <= 3; n++) {
            ResultSet r1 = select_values(t, conds, n);
            ResultSet r2 = select_scan(t, conds, n);
            check(memes_lignes(&r1, &r2), index_kind_name(kind));
            result_free(&r1);
            result_free(&r2);
        }
    }
    db_free(db);
}

int main(void) {
    test_parser();
    test_index(INDEX_HASH);
    test_index(INDEX_AVL);
    test_index(INDEX_LIST);

    if (echecs == 0) {
        printf("Tous les tests sont passes.\n");
        return 0;
    }
    printf("%d test(s) en echec.\n", echecs);
    return 1;
}
