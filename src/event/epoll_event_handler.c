#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/epoll.h>

#include "gnb_event.h"
#include "gnb_fixed_pool.h"

#define MAX_EPOLL_FD 8192

typedef struct _epoll_handler_ctx_t{

    gnb_heap_t *heap;

    gnb_fixed_pool_t *epoll_event_fixed_pool;


    int epoll_fd;

    //out event list for epoll
    struct epoll_event eventlist[MAX_GET_GNB_EVENT];

    gnb_event_array_t *event_array;


}epoll_handler_ctx_t;


static int init_event(gnb_event_cmd *event_cmd, size_t max_event) {

    epoll_handler_ctx_t *handler_ctx = gnb_heap_alloc(event_cmd->heap, sizeof(epoll_handler_ctx_t));

    memset(handler_ctx,0,sizeof(epoll_handler_ctx_t));

    handler_ctx->epoll_event_fixed_pool = gnb_fixed_pool_create(event_cmd->heap, max_event, sizeof(struct epoll_event));

    handler_ctx->epoll_fd = epoll_create(MAX_EPOLL_FD);

    if (-1 == handler_ctx->epoll_fd) {
        perror("epoll_create");
    }

    event_cmd->event_handler_ctx = handler_ctx;

    return 0;
}


static int add_event(gnb_event_cmd *event_cmd, gnb_event_t *ev, int ev_type) {

    int ret = 0;

    epoll_handler_ctx_t *handler_ctx = event_cmd->event_handler_ctx;

    ev->uevent = gnb_fixed_pool_pop(handler_ctx->epoll_event_fixed_pool);

    struct epoll_event *epoll_ev = (struct epoll_event *)ev->uevent;
    memset(epoll_ev,0,sizeof(struct epoll_event));

    epoll_ev->events = EPOLLRDHUP;

    if ( GNB_EVENT_TYPE_READ & ev_type) {
        epoll_ev->events |= EPOLLIN;
    }

    if ( GNB_EVENT_TYPE_WRITE & ev_type) {
        epoll_ev->events |= EPOLLOUT;
    }
    
    epoll_ev->data.ptr = (void*)ev;

    ret = epoll_ctl(handler_ctx->epoll_fd, EPOLL_CTL_ADD, ev->fd, ev->uevent);

    if (ret != 0) {
        perror("add_event epoll_ctl");
    }

    return 0;
}


static int del_event(gnb_event_cmd *event_cmd, gnb_event_t *ev) {

    int ret;

    epoll_handler_ctx_t *handler_ctx = event_cmd->event_handler_ctx;

    ret = epoll_ctl(handler_ctx->epoll_fd, EPOLL_CTL_DEL, ev->fd, ev->uevent);

    gnb_fixed_pool_push(handler_ctx->epoll_event_fixed_pool, (void *)ev->uevent);

    return 0;

}


static int set_event(gnb_event_cmd *event_cmd, gnb_event_t *ev, int op, int ev_type) {

    epoll_handler_ctx_t *handler_ctx = event_cmd->event_handler_ctx;

    struct epoll_event *epoll_ev = (struct epoll_event *)ev->uevent;

    int ev_op;

    int ret = 0;

    switch (op) {

    case GNB_EVENT_OP_ENABLE:

        if( GNB_EVENT_TYPE_READ & ev_type ){
            epoll_ev->events |= EPOLLIN;
        }

        if( GNB_EVENT_TYPE_WRITE & ev_type ){
            epoll_ev->events |= EPOLLOUT;
        }

        //epoll_ev->events |= EPOLLRDHUP;

        break;

    case GNB_EVENT_OP_DISABLE:


        if ( GNB_EVENT_TYPE_READ & ev_type ) {
            epoll_ev->events &= ~EPOLLIN;
        }

        if ( GNB_EVENT_TYPE_WRITE & ev_type ) {
            epoll_ev->events &= ~EPOLLOUT;
        }

        break;

    default:
        break;

    };

    ev_op = EPOLL_CTL_MOD;

    ret = epoll_ctl(handler_ctx->epoll_fd, ev_op, ev->fd, epoll_ev);

    if (ret != 0) {
        perror("set_event epoll_ctl");
    }

    return 0;

}


static int get_event(gnb_event_cmd *event_cmd, gnb_event_t **ev_lst, int *nevents){

    epoll_handler_ctx_t *handler_ctx = event_cmd->event_handler_ctx;

    gnb_event_t *ev;

    //int timewait = 1;
    int timewait = 1000;

    int i;
    int nepevent = 0;

    nepevent = epoll_wait(handler_ctx->epoll_fd, handler_ctx->eventlist, *nevents, timewait);

    if (nepevent < 0) {
        *nevents = 0;
        perror("epoll_wait");
        return 1;
    }

    *nevents = nepevent;

    for (i = 0; i < nepevent; i++) {

        ev_lst[i] = (gnb_event_t*)handler_ctx->eventlist[i].data.ptr;

        ev_lst[i]->ev_type = GNB_EVENT_TYPE_NONE;

        if ( EPOLLRDHUP & handler_ctx->eventlist[i].events) {
            ev_lst[i]->ev_type  |= GNB_EVENT_TYPE_EOF;
        }

        if ( EPOLLHUP & handler_ctx->eventlist[i].events) {
            ev_lst[i]->ev_type |= GNB_EVENT_TYPE_ERROR;
        }

        if ( EPOLLERR & handler_ctx->eventlist[i].events) {
            ev_lst[i]->ev_type |= GNB_EVENT_TYPE_ERROR;
        }

        if ( EPOLLIN & handler_ctx->eventlist[i].events) {
            ev_lst[i]->ev_type |= GNB_EVENT_TYPE_READ;
        }

        if ( EPOLLOUT & handler_ctx->eventlist[i].events) {
            ev_lst[i]->ev_type |= GNB_EVENT_TYPE_WRITE;
        }

    }/*end for*/

    return 0;
}


static int finish_event(gnb_event_cmd *event_cmd){
    
    epoll_handler_ctx_t *handler_ctx = (epoll_handler_ctx_t *)event_cmd->event_handler_ctx;

    gnb_fixed_pool_release(handler_ctx->heap, handler_ctx->epoll_event_fixed_pool);

    gnb_heap_free(event_cmd->heap, handler_ctx->eventlist);
    gnb_heap_free(event_cmd->heap, event_cmd->event_handler_ctx);
    
    return 0;

}


gnb_event_cmd epoll_event_cmd = {

    .mod_name = "epoll_event",

    .heap = NULL,

    .self = &epoll_event_cmd,

    .init_event = init_event,

    .add_event  = add_event,
    .set_event  = set_event,
    .del_event  = del_event,
    .get_event  = get_event,

    .finish_event = finish_event,

    .event_handler_ctx = NULL

};


