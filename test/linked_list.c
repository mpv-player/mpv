#include "test_helpers.h"

#include "common/common.h"
#include "misc/linked_list.h"

struct list_item {
    int v;
    struct {
        struct list_item *prev, *next;
    } list_node;
};

struct the_list {
    struct list_item *head, *tail;
};

static bool do_check_list(struct the_list *lst, int *c, int num_c)
{
    if (!lst->head)
        assert_true(!lst->tail);
    if (!lst->tail)
        assert_true(!lst->head);

    for (struct list_item *cur = lst->head; cur; cur = cur->list_node.next) {
        if (cur->list_node.prev) {
            assert_true(cur->list_node.prev->list_node.next == cur);
            assert_true(lst->head != cur);
        } else {
            assert_true(lst->head == cur);
        }
        if (cur->list_node.next) {
            assert_true(cur->list_node.next->list_node.prev == cur);
            assert_true(lst->tail != cur);
        } else {
            assert_true(lst->tail == cur);
        }

        if (num_c < 1)
            return false;
        if (c[0] != cur->v)
            return false;

        num_c--;
        c++;
    }

    if (num_c)
        return false;

    return true;
}

static void test_linked_list(void **state)
{
    struct the_list lst = {0};
    struct list_item e1 = {1};
    struct list_item e2 = {2};
    struct list_item e3 = {3};
    struct list_item e4 = {4};
    struct list_item e5 = {5};
    struct list_item e6 = {6};

#define check_list(...) \
        assert_true(do_check_list(&lst, (int[]){__VA_ARGS__}, \
                        sizeof((int[]){__VA_ARGS__}) / sizeof(int)));
#define check_list_empty() \
        assert_true(do_check_list(&lst, NULL, 0));

    check_list_empty();
    LL_APPEND(list_node, &lst, &e1);

    check_list(1);
    LL_APPEND(list_node, &lst, &e2);

    check_list(1, 2);
    LL_APPEND(list_node, &lst, &e4);

    check_list(1, 2, 4);
    LL_CLEAR(list_node, &lst);

    check_list_empty();
    LL_PREPEND(list_node, &lst, &e4);

    check_list(4);
    LL_PREPEND(list_node, &lst, &e2);

    check_list(2, 4);
    LL_PREPEND(list_node, &lst, &e1);

    check_list(1, 2, 4);
    LL_CLEAR(list_node, &lst);

    check_list_empty();
    LL_INSERT_BEFORE(list_node, &lst, (struct list_item *)NULL, &e6);

    check_list(6);
    LL_INSERT_BEFORE(list_node, &lst, (struct list_item *)NULL, &e1);

    check_list(6, 1);
    LL_INSERT_BEFORE(list_node, &lst, (struct list_item *)NULL, &e2);

    check_list(6, 1, 2);
    LL_INSERT_BEFORE(list_node, &lst, &e6, &e3);

    check_list(3, 6, 1, 2);
    LL_INSERT_BEFORE(list_node, &lst, &e6, &e5);

    check_list(3, 5, 6, 1, 2);
    LL_INSERT_BEFORE(list_node, &lst, &e2, &e4);

    check_list(3, 5, 6, 1, 4, 2);
    LL_REMOVE(list_node, &lst, &e6);

    check_list(3, 5, 1, 4, 2);
    LL_REMOVE(list_node, &lst, &e3);

    check_list(5, 1, 4, 2);
    LL_REMOVE(list_node, &lst, &e2);

    check_list(5, 1, 4);
    LL_REMOVE(list_node, &lst, &e4);

    check_list(5, 1);
    LL_REMOVE(list_node, &lst, &e5);

    check_list(1);
    LL_REMOVE(list_node, &lst, &e1);

    check_list_empty();
    LL_APPEND(list_node, &lst, &e2);

    check_list(2);
    LL_REMOVE(list_node, &lst, &e2);

    check_list_empty();
    LL_INSERT_AFTER(list_node, &lst, (struct list_item *)NULL, &e1);

    check_list(1);
    LL_INSERT_AFTER(list_node, &lst, (struct list_item *)NULL, &e2);

    check_list(2, 1);
    LL_INSERT_AFTER(list_node, &lst, (struct list_item *)NULL, &e3);

    check_list(3, 2, 1);
    LL_INSERT_AFTER(list_node, &lst, &e3, &e4);

    check_list(3, 4, 2, 1);
    LL_INSERT_AFTER(list_node, &lst, &e4, &e5);

    check_list(3, 4, 5, 2, 1);
    LL_INSERT_AFTER(list_node, &lst, &e1, &e6);

    check_list(3, 4, 5, 2, 1, 6);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_linked_list),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

