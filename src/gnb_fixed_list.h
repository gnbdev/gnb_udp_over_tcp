#ifndef gnb_fixed_list_h
#define gnb_fixed_list_h

#include "gnb_alloc.h"

typedef struct _gnb_fixed_list_data_t{

    uint32_t idx;
    void *udata;
    
}gnb_fixed_list_data_t;


typedef struct _gnb_fixed_list_t{
    
    gnb_heap_t *heap;
    
    uint32_t size;
    
    uint32_t num;
    
    void *block;
    
    gnb_fixed_list_data_t **array;
    
}gnb_fixed_list_t;


gnb_fixed_list_t* gnb_fixed_list_create(gnb_heap_t *heap,uint32_t size);

void gnb_fixed_list_release(gnb_fixed_list_t *list);

gnb_fixed_list_data_t* gnb_fixed_list_push(gnb_fixed_list_t *list, void *udata);

void gnb_fixed_list_pop(gnb_fixed_list_t *list, gnb_fixed_list_data_t *fixed_list_node);

#define GNB_FIX_LIST_UDATA(list_data) (list_data!=NULL?list_data->udata:NULL)

#endif
