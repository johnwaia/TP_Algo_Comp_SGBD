#ifndef INDEX_H
#define INDEX_H

#include "minidb.h"

typedef struct HashTable HashTable;
typedef struct AvlNode AvlNode;
typedef struct ListNode ListNode;

struct Index {
    IndexKind kind;
    HashTable *hash;
    AvlNode *avl;
    ListNode *list;
};

HashTable *hash_new(void);
void hash_insert(HashTable *h, Value key, size_t id);
void hash_find(HashTable *h, Value key, ResultSet *out);
void hash_free(HashTable *h);

AvlNode *avl_insert(AvlNode *n, Value key, size_t id);
void avl_find(AvlNode *n, Value key, ResultSet *out);
void avl_find_range(AvlNode *n, const Condition *c, ResultSet *out);
void avl_free(AvlNode *n);

ListNode *list_insert(ListNode *head, Value key, size_t id);
void list_find(ListNode *head, Value key, ResultSet *out);
void list_find_range(ListNode *head, const Condition *c, ResultSet *out);
void list_free(ListNode *head);

int value_cmp(Value a, Value b);
Value value_copy(Value v);
void value_free(Value v);
int value_match(Value v, const Condition *c);

void result_push(ResultSet *rs, size_t id);
void result_push_all(ResultSet *rs, const ResultSet *src);

#endif
