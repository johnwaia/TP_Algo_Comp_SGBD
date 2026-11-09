#include <stdlib.h>

#include "index.h"

struct ListNode {
    Value key;
    ResultSet ids;
    ListNode *next;
};

ListNode *list_insert(ListNode *head, Value key, size_t id) {
    ListNode *prev = NULL;
    ListNode *cur = head;

    while (cur != NULL && value_cmp(cur->key, key) < 0) {
        prev = cur;
        cur = cur->next;
    }

    if (cur != NULL && value_cmp(cur->key, key) == 0) {
        result_push(&cur->ids, id);
        return head;
    }

    ListNode *node = calloc(1, sizeof(ListNode));
    node->key = value_copy(key);
    result_push(&node->ids, id);
    node->next = cur;

    if (prev == NULL) return node;
    prev->next = node;
    return head;
}

void list_find(ListNode *head, Value key, ResultSet *out) {
    ListNode *cur = head;
    while (cur != NULL && value_cmp(cur->key, key) < 0) {
        cur = cur->next;
    }
    if (cur != NULL && value_cmp(cur->key, key) == 0) {
        result_push_all(out, &cur->ids);
    }
}

void list_find_range(ListNode *head, const Condition *c, ResultSet *out) {
    for (ListNode *cur = head; cur != NULL; cur = cur->next) {
        if (value_match(cur->key, c)) {
            result_push_all(out, &cur->ids);
        }
    }
}

void list_free(ListNode *head) {
    while (head != NULL) {
        ListNode *next = head->next;
        value_free(head->key);
        result_free(&head->ids);
        free(head);
        head = next;
    }
}
