#include "gnb_fixed_list_store.h"

#include <stdlib.h>

gnb_fixed_list_store_t* gnb_fixed_list_store_create(gnb_heap_t *heap,uint32_t size, uint32_t block_size){

    gnb_fixed_list_store_t *fixed_list_store = (gnb_fixed_list_store_t *)gnb_heap_alloc(heap, sizeof(gnb_fixed_list_store_t) + size*sizeof(void*) + block_size *size);

    int i;

    void *p = fixed_list_store->array + size*sizeof(void*);

    for( i=0; i<size; i++){
        p += block_size;
        fixed_list_store->array[i] = p;
    }

    fixed_list_store->num  = 0;
    fixed_list_store->size = size;

    return fixed_list_store;

}


void gnb_fixed_list_store_release(gnb_heap_t *heap, gnb_fixed_list_store_t *fixed_list_store){
    gnb_heap_free(heap,fixed_list_store);
}

