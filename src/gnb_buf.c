#include <stdlib.h>
#include <stddef.h>

#include "gnb_buf.h"


gnb_zbuf_t* gnb_zbuf_heap_alloc(gnb_heap_t *heap, size_t size){

    gnb_zbuf_t *zbuf = (gnb_zbuf_t *)gnb_heap_alloc(heap, sizeof(gnb_zbuf_t) + size );

    zbuf->start = zbuf->block;

    zbuf->end   = zbuf->start + size;

    zbuf->pos   = zbuf->start;
    zbuf->las   = zbuf->start;

    return zbuf;

}


void gnb_zbuf_reset(gnb_zbuf_t *zbuf){

    zbuf->pos   = zbuf->start;
    zbuf->las   = zbuf->start;

}

