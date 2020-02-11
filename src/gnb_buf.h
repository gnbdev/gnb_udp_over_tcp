#ifndef gnb_buf_t_h
#define gnb_buf_t_h

#include "gnb_alloc.h"

typedef struct _gnb_buf_t{

	unsigned char *start;
	unsigned char *end;

	unsigned char *pos;
	unsigned char *las;

}gnb_buf_t;


//the z mean Arrays of Length Zero
typedef struct _gnb_zbuf_t{

	unsigned char *start;
	unsigned char *end;

	unsigned char *pos;
	unsigned char *las;

	unsigned char block[0];

}gnb_zbuf_t;


#define GNB_BUF_LEN(b)     (int)(b->las - b->pos)

#define GNB_BUF_REMAIN(b)  (int)(b->end - b->las)

#define GNB_BUF_SIZE(b)    (int)(b->end - b->start)


#define GNB_BUF_RESET(zbuf)   zbuf->pos=zbuf->las=zbuf->start


gnb_zbuf_t* gnb_zbuf_create(size_t size);

gnb_zbuf_t* gnb_zbuf_heap_alloc(gnb_heap_t *heap, size_t size);




#endif

