#include <assert.h>

#include "narray.h"

narray_t *
narray_new(unsigned element_size, unsigned capacity)
{
    narray_t *na = (narray_t *)malloc(sizeof(narray_t));
    assert(na != NULL);
    na->element_size_in_bytes = element_size;
    na->len_in_bytes = 0;
    na->capacity_in_bytes = capacity * element_size;
    na->data = calloc(capacity, element_size);
    assert(na->data != NULL);
    return na;
}

void
narray_append_val(narray_t *na, const void *value)
{
    if (na->len_in_bytes == na->capacity_in_bytes) {
        unsigned new_capacity = na->capacity_in_bytes + na->capacity_in_bytes +
                                10 * na->element_size_in_bytes;
        void *ndata = calloc(new_capacity, 1);
        assert(ndata != NULL);
        memcpy(ndata, na->data, na->len_in_bytes);
        free(na->data);
        na->data = ndata;
        na->capacity_in_bytes = new_capacity;
    }
    memcpy((char *)na->data + na->len_in_bytes,
           value,
           na->element_size_in_bytes);
    na->len_in_bytes += na->element_size_in_bytes;
}

void
narray_free(narray_t *na)
{
    free(na->data);
    free(na);
}

void
narray_print(narray_t *na, void (*show_element)(void *, int, FILE *), FILE *fp)
{
    mdebug(fprintf(fp, "enter narray_print len=%u\n", na->len);) unsigned len =
        narray_get_len(na);
    unsigned int i;
    for (i = 0; i < len; i++) {
        show_element(na->data, i, fp);
        mdebug(printf("%s ", ((HKEY *)ga->data)[i]);)
    }
}

narray_t *
narray_heaparray_new(void *data,
                     const unsigned len,
                     const unsigned element_size)
{
    narray_t *na = (narray_t *)malloc(sizeof(narray_t));
    assert(na != NULL);
    na->data = data;
    na->len_in_bytes = len;
    na->capacity_in_bytes = len;
    na->element_size_in_bytes = element_size;
    return na;
}
