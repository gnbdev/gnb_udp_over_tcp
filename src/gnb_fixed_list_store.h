#ifndef gnb_fixed_list_store_h
#define gnb_fixed_list_store_h

#include "gnb_alloc.h"

typedef struct _gnb_fixed_list_store_t{

    uint32_t num;
    uint32_t size;

    void *array[0];

}gnb_fixed_list_store_t;

gnb_fixed_list_store_t* gnb_fixed_list_store_create(gnb_heap_t *heap,uint32_t size, uint32_t block_size);

void gnb_fixed_list_store_release(gnb_heap_t *heap, gnb_fixed_list_store_t *fixed_list_store);

#endif
