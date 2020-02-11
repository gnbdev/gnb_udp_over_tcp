#ifndef gnb_event_h
#define gnb_event_h

#include "gnb_alloc.h"

#define MAX_GET_GNB_EVENT       100


#define GNB_EVENT_OP_ADD        (0x0)
#define GNB_EVENT_OP_DEL        (0x1)
#define GNB_EVENT_OP_ENABLE     (0x1 << 1)
#define GNB_EVENT_OP_DISABLE    (0x1 << 2)


typedef struct _gnb_event_t {

	int fd;

    #define GNB_EVENT_FD_TCP4_LISTEN     0x8
	#define GNB_EVENT_FD_TCP6_LISTEN     0x4
    #define GNB_EVENT_FD_TCPV4_CONNECT   0x3
    #define GNB_EVENT_FD_TCPV6_CONNECT   0x1
    #define GNB_EVENT_FD_UDP4_SOCKET     0x2
    #define GNB_EVENT_FD_UDP6_SOCKET     0x7
	int fd_type;

	void *udata;

    #define GNB_EVENT_TYPE_NONE          (0x0)
    #define GNB_EVENT_TYPE_READ          (0x1)
    #define GNB_EVENT_TYPE_WRITE         (0x1 << 1)
    #define GNB_EVENT_TYPE_TIMER         (0x1 << 2)
    #define GNB_EVENT_TYPE_EOF           (0x1 << 3)
    #define GNB_EVENT_TYPE_ERROR         (0x1 << 4)


	//在处理事件过程中如果遇到这个标记位意味着 这个事件已经处理完，event已经放回 event_fixed_pool 中，该事件不需要再处理了
	//出现这种情况的原因是在 gnb_get_event_cmd 过程中 一个conn有1个以上的事件出现在ev_lst中，例如在 kqueue中可能 同时有读与写的事件又或者一个写事件和连接断开的事件，
	//当处理 handler 在处理第一个事件时关闭释放了这个 conn 的资源，会把event标记为GNB_EVENT_TYPE_FINISH，并把 event 放回 event_fixed_pool
	//此时 这个 event 对应内存块虽然可以访问，但是这个event已不再关联到conn等资源了，GNB_EVENT_TYPE_FINISH 这个标志位就是告知事件处理过程不要处理这个event。
    #define GNB_EVENT_TYPE_FINISH        (0x1 << 5)
	int ev_type;

	int index;

	void *uevent;

}gnb_event_t;


typedef struct _gnb_event_array_t{

    size_t size;

    size_t nevent;

    gnb_event_t    *list[0];

}gnb_event_array_t;


typedef struct _gnb_event_cmd gnb_event_cmd;


typedef int (*gnb_init_event_cmd)(gnb_event_cmd *event_cmd, size_t max_event);

typedef int (*gnb_add_event_cmd)(gnb_event_cmd *event_cmd, gnb_event_t *ev, int ev_type);

typedef int (*gnb_set_event_cmd)(gnb_event_cmd *event_cmd, gnb_event_t *ev, int op, int ev_type);

typedef int (*gnb_del_event_cmd)(gnb_event_cmd *event_cmd, gnb_event_t *ev);

typedef int (*gnb_get_event_cmd)(gnb_event_cmd *event_cmd, gnb_event_t **ev_lst, int *num_ev);

typedef int (*gnb_finish_event_cmd)(gnb_event_cmd *event_cmd);

typedef struct _gnb_event_cmd {

  char *mod_name;

  gnb_heap_t *heap;

  struct _gnb_event_cmd *self;

  gnb_init_event_cmd    init_event;
  gnb_add_event_cmd     add_event;
  gnb_set_event_cmd     set_event;
  gnb_del_event_cmd     del_event;
  gnb_get_event_cmd     get_event;
  gnb_finish_event_cmd  finish_event;

  void *event_handler_ctx;

}gnb_event_cmd;

gnb_event_cmd* gnb_create_event_cmd();

#endif



