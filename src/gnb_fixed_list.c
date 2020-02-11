#include <stdint.h>
#include "stdlib.h"
#include "string.h"

#include "gnb_fixed_list.h"


gnb_fixed_list_t* gnb_fixed_list_create(gnb_heap_t *heap,uint32_t size){
    
    uint32_t i;

    gnb_fixed_list_t *list = (gnb_fixed_list_t *)gnb_heap_alloc(heap, sizeof(gnb_fixed_list_t));

    memset(list, 0, sizeof(gnb_fixed_list_t));

    list->heap = heap;
    
    list->block = (gnb_fixed_list_data_t *)gnb_heap_alloc(heap, sizeof(gnb_fixed_list_data_t) * size);

    list->array = (gnb_fixed_list_data_t **)gnb_heap_alloc(heap, sizeof(gnb_fixed_list_data_t *) * size);
    
    list->size = size;
    list->num  = 0;

    void *p;

    p = list->block;

    for (i=0; i<list->size; i++) {
        list->array[i] = p;
        p += sizeof(gnb_fixed_list_data_t);
    }
    
    return list;
    
}


gnb_fixed_list_data_t* gnb_fixed_list_push(gnb_fixed_list_t *list, void *udata){
    
    if ( list->size == list->num ) {
        return NULL;
    }

    gnb_fixed_list_data_t *fixed_list_node = list->array[list->num];

    fixed_list_node->udata = udata;
    
    fixed_list_node->idx = list->num;
    
    list->num++;

    return fixed_list_node;

}


void gnb_fixed_list_pop(gnb_fixed_list_t *list, gnb_fixed_list_data_t *fixed_list_node){

    if ( 0 == list->num ) {
        //发生错误了
        return;
    }

    if ( 1 == list->num ) {
        list->num = 0;
        return;
    }

    gnb_fixed_list_data_t *last_node = list->array[list->num - 1];

    if ( fixed_list_node->idx > (list->size-1)  ) {
        //发生错误了
        return;
    }

    if ( last_node->idx > (list->size-1) ) {
        //发生错误了
        return;
    }

    if ( 1 == list->num ) {
        if ( fixed_list_node->idx != last_node->idx ){
            //发生错误了
            return;
        }
        goto finish;
    }

    if( last_node->idx == fixed_list_node->idx ){
        goto finish;
    }


    last_node->idx = fixed_list_node->idx;

    list->array[last_node->idx] = last_node;

    
finish:

    list->num--;
    return;
    
}


void gnb_fixed_list_release(gnb_fixed_list_t *list){
    
    gnb_heap_free(list->heap,list->block);
    gnb_heap_free(list->heap,list->array);
    gnb_heap_free(list->heap,list);
    
}
