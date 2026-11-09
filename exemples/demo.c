#include <stdio.h>

#include "minidb.h"

int main(void) {
    Database *db = db_create();

    Table *users = create_table_struct(db, "users(id INT, name TEXT(24), city TEXT(16), age INT)");
    if (users == NULL) {
        fprintf(stderr, "definition invalide\n");
        return 1;
    }

    struct { int id; const char *name, *city; int age; } data[] = {
        {1, "Alice", "Paris", 31},   {2, "Bob", "Lyon", 25},
        {3, "Chloe", "Paris", 42},   {4, "David", "Lille", 19},
        {5, "Emma", "Lyon", 37},     {6, "Farid", "Paris", 25},
        {7, "Gaelle", "Nantes", 55}, {8, "Hugo", "Lille", 28},
    };
    for (size_t i = 0; i < sizeof data / sizeof data[0]; i++) {
        Value row[] = { VAL_INT(data[i].id), VAL_TEXT(data[i].name),
                        VAL_TEXT(data[i].city), VAL_INT(data[i].age) };
        insert_row(users, row, 4);
    }

    printf("== SELECT * FROM users WHERE city = 'Paris'\n");
    Condition c1[] = { { "city", OP_EQ, VAL_TEXT("Paris"), {0} } };
    ResultSet rs = select_values(users, c1, 1);
    print_rows(users, &rs, NULL, 0);
    result_free(&rs);

    create_index(users, "id", INDEX_HASH);
    create_index(users, "age", INDEX_AVL);
    create_index(users, "city", INDEX_HASH);

    printf("== SELECT name, age FROM users WHERE id = 5\n");
    Condition c2[] = { { "id", OP_EQ, VAL_INT(5), {0} } };
    const char *proj[] = { "name", "age" };
    rs = select_values(users, c2, 1);
    print_rows(users, &rs, proj, 2);
    result_free(&rs);

    printf("== SELECT * FROM users WHERE age BETWEEN 25 AND 40\n");
    Condition c3[] = { { "age", OP_BETWEEN, VAL_INT(25), VAL_INT(40) } };
    rs = select_values(users, c3, 1);
    print_rows(users, &rs, NULL, 0);
    result_free(&rs);

    printf("== SELECT name FROM users WHERE city = 'Paris' AND age < 40\n");
    Condition c4[] = { { "city", OP_EQ, VAL_TEXT("Paris"), {0} },
                       { "age", OP_LT, VAL_INT(40), {0} } };
    const char *proj2[] = { "name" };
    rs = select_values(users, c4, 2);
    print_rows(users, &rs, proj2, 1);
    result_free(&rs);

    printf("== Definition invalide : %s\n",
           create_table_struct(db, "bad(id FLOAT)") ? "acceptee" : "refusee");
    Value mauvaise[] = { VAL_TEXT("x"), VAL_TEXT("y"), VAL_TEXT("z"), VAL_INT(1) };
    printf("== Insertion avec mauvais type : %s\n",
           insert_row(users, mauvaise, 4) == 0 ? "acceptee" : "refusee");

    db_free(db);
    return 0;
}
