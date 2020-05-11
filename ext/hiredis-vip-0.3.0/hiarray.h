#ifndef __HIARRAY_H_
#define __HIARRAY_H_

#include <stdio.h>

typedef int (*hiarray_compare_t)(const void *, const void *);
typedef int (*hiarray_each_t)(void *, void *);

struct hiarray {
    uint32_t nelem;  /* # element */
    void     *elem;  /* element */
    size_t   size;   /* element size */
    uint32_t nalloc; /* # allocated element */
};

#define null_hiarray { 0, NULL, 0, 0 }

static inline void
hiarray_null(struct hiarray *a)
{
    a->nelem = 0;
    a->elem = NULL;
    a->size = 0;
    a->nalloc = 0;
}

static inline void
hiarray_set(struct hiarray *a, void *elem, size_t size, uint32_t nalloc)
{
    a->nelem = 0;
    a->elem = elem;
    a->size = size;
    a->nalloc = nalloc;
}

static inline uint32_t
hiarray_n(const struct hiarray *a)
{
    return a->nelem;
}

struct hiarray *hiarray_create(uint32_t n, size_t size);
void hiarray_destroy(struct hiarray *a);
int hiarray_init(struct hiarray *a, uint32_t n, size_t size);
void hiarray_deinit(struct hiarray *a);

uint32_t hiarray_idx(struct hiarray *a, void *elem);
void *hiarray_push(struct hiarray *a);
void *hiarray_pop(struct hiarray *a);
void *hiarray_get(struct hiarray *a, uint32_t idx);
void *hiarray_top(struct hiarray *a);
void hiarray_swap(struct hiarray *a, struct hiarray *b);
void hiarray_sort(struct hiarray *a, hiarray_compare_t compare);
int hiarray_each(struct hiarray *a, hiarray_each_t func, void *data);

#endif
