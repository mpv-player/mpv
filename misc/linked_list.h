#pragma once

#include <stddef.h>

/*
 * Doubly linked list macros. All of these require that each list item is a
 * struct, that contains a field, that is another struct with prev/next fields:
 *
 *  struct example_item {
 *      struct {
 *          struct example_item *prev, *next;
 *      } mylist;
 *  };
 *
 * And a struct somewhere that represents the "list" and has head/tail fields:
 *
 *  struct {
 *      struct example_item *head, *tail;
 *  } mylist_var;
 *
 * Then you can e.g. insert elements like this:
 *
 *  struct example_item item;
 *  LL_APPEND(mylist, &mylist_var, &item);
 *
 * The first macro argument is always the name if the field in the item that
 * contains the prev/next pointers, in this case struct example_item.mylist.
 * This was done so that a single item can be in multiple lists.
 *
 * The list is started/terminated with NULL. Nothing ever points _to_ the
 * list head, so the list head memory location can be safely moved.
 *
 * General rules are:
 *  - list head is initialized by setting head/tail to NULL
 *  - list items do not need to be initialized before inserting them
 *  - next/prev fields of list items are not cleared when they are removed
 *  - there's no way to know whether an item is in the list or not (unless
 *    you clear prev/next on init/removal, _and_ check whether items with
 *    prev/next==NULL are referenced by head/tail)
 */

// Insert item at the end of the list (list->tail == item).
// Undefined behavior if item is already in the list.
#define LL_APPEND(field, list, item)  do {                              \
    (item)->field.prev = (list)->tail;                                  \
    (item)->field.next = NULL;                                          \
    LL_RELINK_(field, list, item)                                       \
} while (0)

// Insert item enew after eprev (i.e. eprev->next == enew). If eprev is NULL,
// then insert it as head (list->head == enew).
// Undefined behavior if enew is already in the list, or eprev isn't.
#define LL_INSERT_AFTER(field, list, eprev, enew) do {                  \
    (enew)->field.prev = (eprev);                                       \
    (enew)->field.next = (eprev) ? (eprev)->field.next : (list)->head;  \
    LL_RELINK_(field, list, enew)                                       \
} while (0)

// Insert item at the start of the list (list->head == item).
// Undefined behavior if item is already in the list.
#define LL_PREPEND(field, list, item)  do {                             \
    (item)->field.prev = NULL;                                          \
    (item)->field.next = (list)->head;                                  \
    LL_RELINK_(field, list, item)                                       \
} while (0)

// Insert item enew before enext (i.e. enew->next == enext). If enext is NULL,
// then insert it as tail (list->tail == enew).
// Undefined behavior if enew is already in the list, or enext isn't.
#define LL_INSERT_BEFORE(field, list, enext, enew) do {                 \
    (enew)->field.prev = (enext) ? (enext)->field.prev : (list)->tail;  \
    (enew)->field.next = (enext);                                       \
    LL_RELINK_(field, list, enew)                                       \
} while (0)

// Remove the item from the list.
// Undefined behavior if item is not in the list.
#define LL_REMOVE(field, list, item) do {                               \
    if ((item)->field.prev) {                                           \
        (item)->field.prev->field.next = (item)->field.next;            \
    } else {                                                            \
        (list)->head = (item)->field.next;                              \
    }                                                                   \
    if ((item)->field.next) {                                           \
        (item)->field.next->field.prev = (item)->field.prev;            \
    } else {                                                            \
        (list)->tail = (item)->field.prev;                              \
    }                                                                   \
} while (0)

// Remove all items from the list.
#define LL_CLEAR(field, list) do {                                      \
    (list)->head = (list)->tail = NULL;                                 \
} while (0)

// Internal helper.
#define LL_RELINK_(field, list, item)                                   \
    if ((item)->field.prev) {                                           \
        (item)->field.prev->field.next = (item);                        \
    } else {                                                            \
        (list)->head = (item);                                          \
    }                                                                   \
    if ((item)->field.next) {                                           \
        (item)->field.next->field.prev = (item);                        \
    } else {                                                            \
        (list)->tail = (item);                                          \
    }
