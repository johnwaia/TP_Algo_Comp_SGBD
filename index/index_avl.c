#include <stdlib.h>

#include "index.h"

struct AvlNode {
    Value key;
    ResultSet ids;
    AvlNode *left;
    AvlNode *right;
    int height;
};

static int height(AvlNode *n) {
    if (n == NULL) return 0;
    return n->height;
}

static void update_height(AvlNode *n) {
    int hl = height(n->left);
    int hr = height(n->right);
    n->height = 1 + (hl > hr ? hl : hr);
}

static AvlNode *rotate_right(AvlNode *y) {
    AvlNode *x = y->left;
    y->left = x->right;
    x->right = y;
    update_height(y);
    update_height(x);
    return x;
}

static AvlNode *rotate_left(AvlNode *x) {
    AvlNode *y = x->right;
    x->right = y->left;
    y->left = x;
    update_height(x);
    update_height(y);
    return y;
}

AvlNode *avl_insert(AvlNode *n, Value key, size_t id) {
    if (n == NULL) {
        AvlNode *node = calloc(1, sizeof(AvlNode));
        node->key = value_copy(key);
        node->height = 1;
        result_push(&node->ids, id);
        return node;
    }

    int c = value_cmp(key, n->key);
    if (c == 0) {
        result_push(&n->ids, id);
        return n;
    }
    if (c < 0) {
        n->left = avl_insert(n->left, key, id);
    } else {
        n->right = avl_insert(n->right, key, id);
    }

    update_height(n);
    int balance = height(n->left) - height(n->right);

    if (balance > 1) {
        if (value_cmp(key, n->left->key) > 0) {
            n->left = rotate_left(n->left);
        }
        return rotate_right(n);
    }
    if (balance < -1) {
        if (value_cmp(key, n->right->key) < 0) {
            n->right = rotate_right(n->right);
        }
        return rotate_left(n);
    }
    return n;
}

void avl_find(AvlNode *n, Value key, ResultSet *out) {
    while (n != NULL) {
        int c = value_cmp(key, n->key);
        if (c == 0) {
            result_push_all(out, &n->ids);
            return;
        }
        if (c < 0) n = n->left;
        else n = n->right;
    }
}

void avl_find_range(AvlNode *n, const Condition *c, ResultSet *out) {
    if (n == NULL) return;

    int has_min = c->op == OP_GT || c->op == OP_GE || c->op == OP_BETWEEN;
    int has_max = c->op == OP_LT || c->op == OP_LE || c->op == OP_BETWEEN;
    Value max = c->op == OP_BETWEEN ? c->v2 : c->v1;

    if (!has_min || value_cmp(n->key, c->v1) > 0) {
        avl_find_range(n->left, c, out);
    }
    if (value_match(n->key, c)) {
        result_push_all(out, &n->ids);
    }
    if (!has_max || value_cmp(n->key, max) < 0) {
        avl_find_range(n->right, c, out);
    }
}

void avl_free(AvlNode *n) {
    if (n == NULL) return;
    avl_free(n->left);
    avl_free(n->right);
    value_free(n->key);
    result_free(&n->ids);
    free(n);
}
