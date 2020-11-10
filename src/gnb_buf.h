#ifndef gnb_buf_t_h
#define gnb_buf_t_h

#include "gnb_alloc.h"

#include "gnb_buf_type.h"

gnb_zbuf_t* gnb_zbuf_create(size_t size);
gnb_zbuf_t* gnb_zbuf_heap_alloc(gnb_heap_t *heap, size_t size);

#endif
