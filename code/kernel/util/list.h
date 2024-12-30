#ifndef __UTIL_LISH_H__
#define __UTIL_LISH_H__

#include "types.h"
typedef struct list_entry List_entry;
struct list_entry {
    List_entry *prev;
    List_entry *next;
};

#define list_for_each(le, head) for ((le) = (head)->next; (le) != (head); (le) = (le)->next)

#define list_for_each_prev(le, head) for ((le) = (head)->prev; (le) != (head); (le) = (le)->prev)

static inline void list_init(List_entry *elem) __attribute__((always_inline));
static inline void list_add(List_entry *listelem, List_entry *elem) __attribute__((always_inline));
static inline void list_add_before(List_entry *listelem, List_entry *elem) __attribute__((always_inline));
static inline void list_add_after(List_entry *listelem, List_entry *elem) __attribute__((always_inline));
static inline void list_del(List_entry *listelem) __attribute__((always_inline));
static inline void list_del_init(List_entry *listelem) __attribute__((always_inline));
static inline bool list_empty(List_entry *list) __attribute__((always_inline));
static inline List_entry *list_next(List_entry *listelem) __attribute__((always_inline));
static inline List_entry *list_prev(List_entry *listelem) __attribute__((always_inline));

static size_t list_count(List_entry *elem) {
    size_t count = 0;
    List_entry *le = list_next(elem);
    while (le != elem) {
        count++; 
        le = list_next(le);
    }
    return count;
}

static inline void __list_add(List_entry *elem, List_entry *prev, List_entry *next)
    __attribute__((always_inline));
static inline void __list_del(List_entry *prev, List_entry *next) __attribute__((always_inline));

static inline void list_init(List_entry *elem) { elem->prev = elem->next = elem; }

static inline void __list_add(List_entry *elem, List_entry *prev, List_entry *next) {
    prev->next = next->prev = elem;
    elem->next = next;
    elem->prev = prev;
}

// insert at last positon
static inline void list_add_before(List_entry *listelem, List_entry *elem) {
    __list_add(elem, listelem->prev, listelem);
}

// insert at first position
static inline void list_add_after(List_entry *listelem, List_entry *elem) {
    __list_add(elem, listelem, listelem->next);
}

// insert at last position
static inline void list_add(List_entry *listelem, List_entry *elem) { list_add_before(listelem, elem); }

static inline void __list_del(List_entry *prev, List_entry *next) {
    prev->next = next;
    next->prev = prev;
}

static inline void list_del(List_entry *listelem) { __list_del(listelem->prev, listelem->next); }

static inline void list_del_init(List_entry *listelem) {
    list_del(listelem);
    list_init(listelem);
}

static inline bool list_empty(List_entry *list) { return list->next == list; }

static inline List_entry *list_next(List_entry *listelem) { return listelem->next; }

static inline List_entry *list_prev(List_entry *listelem) { return listelem->prev; }

static inline bool list_contains(List_entry *head, List_entry *elem) {
    List_entry *le;
    list_for_each(le, head) {
        if (le == elem) return true;
    }
    return false;
}

static inline void list_join(List_entry *list1, List_entry *list2) {
    if (list_empty(list2)) { return; }
    list1->prev->next = list2->next;
    list2->next->prev = list1->prev;

    list2->prev->next = list1;
    list1->prev = list2->prev;
    list_init(list2);
}

#endif
