#ifndef __UTIL_LISH_H__
#define __UTIL_LISH_H__

#include "types.h"
typedef struct list_entry List_entry;
struct list_entry{
    List_entry *prev;
    List_entry *next;
};


static inline void list_init(List_entry *elem) __attribute__((always_inline));
static inline void list_add(List_entry *listelem, List_entry *elem) __attribute__((always_inline));
static inline void list_add_before(List_entry *listelem, List_entry *elem) __attribute__((always_inline));
static inline void list_add_after(List_entry *listelem, List_entry *elem) __attribute__((always_inline));
static inline void list_del(List_entry *listelem) __attribute__((always_inline));
static inline void list_del_init(List_entry *listelem) __attribute__((always_inline));
static inline bool list_empty(List_entry *list) __attribute__((always_inline));
static inline List_entry *list_next(List_entry *listelem) __attribute__((always_inline));
static inline List_entry *list_prev(List_entry *listelem) __attribute__((always_inline));

static inline void __list_add(List_entry *elem, List_entry *prev, List_entry *next)
    __attribute__((always_inline));
static inline void __list_del(List_entry *prev, List_entry *next) __attribute__((always_inline));

static inline void list_init(List_entry *elem){
    elem->prev = elem->next = elem;
}

static inline void __list_add(List_entry *elem, List_entry *prev, List_entry *next){
    prev->next = next->prev = elem;
    elem->next = next;
    elem->prev = prev;
}

static inline void list_add_before(List_entry *listelem, List_entry *elem){
    __list_add(elem, listelem->prev, listelem);
}

static inline void list_add_after(List_entry *listelem, List_entry *elem){
    __list_add(elem, listelem, listelem->next);
}

static inline void list_add(List_entry *listelem, List_entry *elem){
    list_add_after(listelem, elem);
}

static inline void __list_del(List_entry *prev, List_entry *next){
    prev->next = next;
    next->prev = prev;
}

static inline void list_del(List_entry *listelem){
    __list_del(listelem->prev, listelem->next);
}

static inline void list_del_init(List_entry *listelem){
    list_del(listelem);
    list_init(listelem);
}

static inline bool list_empty(List_entry *list){
    return list->next = list;
}

static inline List_entry *list_next(List_entry *listelem){
    return listelem->next;
}

static inline List_entry *list_prev(List_entry *listelem){
    return listelem->prev;
}

#endif
