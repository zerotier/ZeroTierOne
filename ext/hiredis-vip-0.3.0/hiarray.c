#include <stdlib.h>

#include "hiutil.h"
#include "hiarray.h"

struct hiarray *
hiarray_create(uint32_t n, size_t size)
{
    struct hiarray *a;

    ASSERT(n != 0 && size != 0);

    a = hi_alloc(sizeof(*a));
    if (a == NULL) {
        return NULL;
    }

    a->elem = hi_alloc(n * size);
    if (a->elem == NULL) {
        hi_free(a);
        return NULL;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return a;
}

void
hiarray_destroy(struct hiarray *a)
{
    hiarray_deinit(a);
    hi_free(a);
}

int
hiarray_init(struct hiarray *a, uint32_t n, size_t size)
{
    ASSERT(n != 0 && size != 0);

    a->elem = hi_alloc(n * size);
    if (a->elem == NULL) {
        return HI_ENOMEM;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return HI_OK;
}

void
hiarray_deinit(struct hiarray *a)
{
    ASSERT(a->nelem == 0);

    if (a->elem != NULL) {
        hi_free(a->elem);
    }
}

uint32_t
hiarray_idx(struct hiarray *a, void *elem)
{
    uint8_t *p, *q;
    uint32_t off, idx;

    ASSERT(elem >= a->elem);

    p = a->elem;
    q = elem;
    off = (uint32_t)(q - p);

    ASSERT(off % (uint32_t)a->size == 0);

    idx = off / (uint32_t)a->size;

    return idx;
}

void *
hiarray_push(struct hiarray *a)
{
    void *elem, *new;
    size_t size;

    if (a->nelem == a->nalloc) {

        /* the array is full; allocate new array */
        size = a->size * a->nalloc;
        new = hi_realloc(a->elem, 2 * size);
        if (new == NULL) {
            return NULL;
        }

        a->elem = new;
        a->nalloc *= 2;
    }

    elem = (uint8_t *)a->elem + a->size * a->nelem;
    a->nelem++;

    return elem;
}

void *
hiarray_pop(struct hiarray *a)
{
    void *elem;

    ASSERT(a->nelem != 0);

    a->nelem--;
    elem = (uint8_t *)a->elem + a->size * a->nelem;

    return elem;
}

void *
hiarray_get(struct hiarray *a, uint32_t idx)
{
    void *elem;

    ASSERT(a->nelem != 0);
    ASSERT(idx < a->nelem);

    elem = (uint8_t *)a->elem + (a->size * idx);

    return elem;
}

void *
hiarray_top(struct hiarray *a)
{
    ASSERT(a->nelem != 0);

    return hiarray_get(a, a->nelem - 1);
}

void
hiarray_swap(struct hiarray *a, struct hiarray *b)
{
    struct hiarray tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Sort nelem elements of the array in ascending order based on the
 * compare comparator.
 */
void
hiarray_sort(struct hiarray *a, hiarray_compare_t compare)
{
    ASSERT(a->nelem != 0);

    qsort(a->elem, a->nelem, a->size, compare);
}

/*
 * Calls the func once for each element in the array as long as func returns
 * success. On failure short-circuits and returns the error status.
 */
int
hiarray_each(struct hiarray *a, hiarray_each_t func, void *data)
{
    uint32_t i, nelem;

    ASSERT(array_n(a) != 0);
    ASSERT(func != NULL);

    for (i = 0, nelem = hiarray_n(a); i < nelem; i++) {
        void *elem = hiarray_get(a, i);
        rstatus_t status;

        status = func(elem, data);
        if (status != HI_OK) {
            return status;
        }
    }

    return HI_OK;
}
