#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include "gnb_event.h"
#include "gnb_fixed_pool.h"

typedef struct _fd_events_t{

    #define READ_KEVENT  0
    #define WRITE_KEVENT 1
    struct kevent changelist[2];
    
}fd_events_t;


typedef struct _kqueue_handler_ctx_t{

    gnb_heap_t *heap;
    
    gnb_fixed_pool_t *fd_events_fixed_pool;

    int kqueue_fd;
    
    struct kevent *eventlist;
    



    gnb_event_array_t *event_array;

}kqueue_handler_ctx_t;



static int init_event(gnb_event_cmd *event_cmd, size_t max_event){

    kqueue_handler_ctx_t *handler_ctx = malloc(sizeof(kqueue_handler_ctx_t));
    
    memset(handler_ctx, 0, sizeof(kqueue_handler_ctx_t));

    handler_ctx->heap = event_cmd->heap;
    
    handler_ctx->kqueue_fd = kqueue();
    
    handler_ctx->fd_events_fixed_pool = gnb_fixed_pool_create(handler_ctx->heap, max_event, sizeof(fd_events_t));
    
    handler_ctx->eventlist  = (struct kevent *)malloc( sizeof(struct kevent) * max_event );
    memset(handler_ctx->eventlist, 0, sizeof(struct kevent) * max_event);

    handler_ctx->event_array = (gnb_event_array_t *)malloc( sizeof(gnb_event_array_t) + sizeof(gnb_event_t) * max_event );

    memset(handler_ctx->event_array, 0, sizeof(gnb_event_array_t) + sizeof(gnb_event_t) * max_event);

    handler_ctx->event_array->size = max_event;

    event_cmd->event_handler_ctx = handler_ctx;

    return 0;
    
}


static int add_event(gnb_event_cmd *event_cmd, gnb_event_t *ev, int ev_type){

    kqueue_handler_ctx_t *handler_ctx = (kqueue_handler_ctx_t *)event_cmd->event_handler_ctx;
        
    if ( handler_ctx->event_array->nevent >= handler_ctx->event_array->size ){
        return -1;
    }

    /* ************************************ */
    handler_ctx->event_array->list[ handler_ctx->event_array->nevent ] = ev;
    ev->index = handler_ctx->event_array->nevent;
    handler_ctx->event_array->nevent++;
    /* ************************************ */
    
    fd_events_t *fd_events = gnb_fixed_pool_pop(handler_ctx->fd_events_fixed_pool);
    if (NULL==fd_events) {
        return -1;
    }

    memset(fd_events, 0, sizeof(fd_events_t));
    
    ev->uevent = fd_events;
    
    fd_events->changelist[READ_KEVENT].ident  = ev->fd;
    fd_events->changelist[READ_KEVENT].filter = EVFILT_READ;

    if ( GNB_EVENT_TYPE_READ & ev_type) {
        fd_events->changelist[READ_KEVENT].flags  = EV_ADD | EV_ENABLE;
    }else{
        fd_events->changelist[READ_KEVENT].flags  = EV_ADD | EV_DISABLE;
    }

    fd_events->changelist[READ_KEVENT].flags  = EV_ADD | EV_ENABLE;
    fd_events->changelist[READ_KEVENT].fflags = 0;
    fd_events->changelist[READ_KEVENT].data   = 0;
    fd_events->changelist[READ_KEVENT].udata  = (void*)ev;
    

    fd_events->changelist[WRITE_KEVENT].ident  = ev->fd;
    fd_events->changelist[WRITE_KEVENT].filter = EVFILT_WRITE;
    
    if ( GNB_EVENT_TYPE_WRITE & ev_type) {
        fd_events->changelist[WRITE_KEVENT].flags  = EV_ADD | EV_ENABLE;
    }else{
        fd_events->changelist[WRITE_KEVENT].flags  = EV_ADD | EV_DISABLE;
    }

    fd_events->changelist[WRITE_KEVENT].fflags = 0;
    fd_events->changelist[WRITE_KEVENT].data   = 0;
    fd_events->changelist[WRITE_KEVENT].udata  = (void*)ev;

    kevent( handler_ctx->kqueue_fd, fd_events->changelist, 2, NULL, 0, NULL );

    return 0;

}


static int set_event(gnb_event_cmd *event_cmd, gnb_event_t *ev, int op, int ev_type){

    kqueue_handler_ctx_t *handler_ctx = (kqueue_handler_ctx_t *)event_cmd->event_handler_ctx;

    fd_events_t *fd_events = (fd_events_t *)ev->uevent;

    uint16_t flags;

    switch (op) {

        case GNB_EVENT_OP_ENABLE:
            flags = EV_ENABLE;
            break;
        case GNB_EVENT_OP_DISABLE:
            flags = EV_DISABLE;
            break;
        default:
            break;

    };
    
    if( GNB_EVENT_TYPE_READ & ev_type ){
        fd_events->changelist[READ_KEVENT].flags = flags;
        kevent( handler_ctx->kqueue_fd, &fd_events->changelist[READ_KEVENT], 1, NULL, 0, NULL );
    }
    
    if( GNB_EVENT_TYPE_WRITE & ev_type ){
        fd_events->changelist[WRITE_KEVENT].flags = flags;
        kevent( handler_ctx->kqueue_fd, &fd_events->changelist[WRITE_KEVENT], 1, NULL, 0, NULL );
    }
    
    return 0;
}


static int del_event(gnb_event_cmd *event_cmd, gnb_event_t *ev){
    
    int ret;
    
    kqueue_handler_ctx_t *handler_ctx = (kqueue_handler_ctx_t *)event_cmd->event_handler_ctx;

    fd_events_t *fd_events = (fd_events_t *)ev->uevent;

    if ( 0 == handler_ctx->event_array->nevent ){
        goto finish;
    }
    
    if ( ev->index > handler_ctx->event_array->nevent ){
        goto finish;
    }

    /* ************************************ */
    gnb_event_t *last_event = handler_ctx->event_array->list[ handler_ctx->event_array->nevent-1 ];
    last_event->index = ev->index;
    handler_ctx->event_array->list[ last_event->index ] = last_event;
    handler_ctx->event_array->nevent--;
    /* ************************************ */

finish:

    fd_events->changelist[READ_KEVENT].flags  = EV_DELETE;
    fd_events->changelist[WRITE_KEVENT].flags = EV_DELETE;

    kevent( handler_ctx->kqueue_fd, fd_events->changelist, 2, NULL, 0, NULL );

    ret = gnb_fixed_pool_push(handler_ctx->fd_events_fixed_pool, (void *)fd_events);
    
    return 0;
}


static int get_event(gnb_event_cmd *event_cmd, gnb_event_t **ev_lst, int *nevents){
    
    kqueue_handler_ctx_t *handler_ctx = (kqueue_handler_ctx_t *)event_cmd->event_handler_ctx;

    struct timespec ts;

    int num_kq_event = 0;

    ts.tv_sec = 1;
    ts.tv_nsec = 10*1000*1000;
    
    num_kq_event = kevent( handler_ctx->kqueue_fd, NULL, 0, handler_ctx->eventlist, *nevents, &ts );

    if ( -1 == num_kq_event ) {
        *nevents = 0;
        return -1;
    }

    if ( num_kq_event < *nevents ) {
        *nevents = num_kq_event;
    }
    
    if (0==num_kq_event) {
        return num_kq_event;
    }
    
    int i;

    for (i=0; i<num_kq_event; i++) {

        ev_lst[i] = (gnb_event_t *)handler_ctx->eventlist[i].udata;

        ev_lst[i]->ev_type = GNB_EVENT_TYPE_NONE;
        
        switch (handler_ctx->eventlist[i].filter) {
                
            case EVFILT_READ:
                ev_lst[i]->ev_type = GNB_EVENT_TYPE_READ;
                break;
                
            case EVFILT_WRITE:
                ev_lst[i]->ev_type = GNB_EVENT_TYPE_WRITE;
                break;

            default:
                break;

        }

        if( handler_ctx->eventlist[i].flags & EV_ERROR ){
            ev_lst[i]->ev_type |= GNB_EVENT_TYPE_ERROR;
        }

        if( handler_ctx->eventlist[i].flags & EV_EOF ){
            ev_lst[i]->ev_type |= GNB_EVENT_TYPE_EOF;
        }

    }
    
    return 0;

}


static int finish_event(gnb_event_cmd *event_cmd){
    
    kqueue_handler_ctx_t *handler_ctx = (kqueue_handler_ctx_t *)event_cmd->event_handler_ctx;

    gnb_fixed_pool_release(handler_ctx->heap, handler_ctx->fd_events_fixed_pool);
        
    free(handler_ctx->eventlist);

    free(handler_ctx->event_array);

    free(event_cmd->event_handler_ctx);
    
    return 0;
}



gnb_event_cmd kqueue_event_cmd = {

    .mod_name = "kqueue_event",

    .heap = NULL,

    .self = &kqueue_event_cmd,

    .init_event = init_event,

    .add_event  = add_event,
    .set_event  = set_event,
    .del_event  = del_event,
    .get_event  = get_event,

    .finish_event = finish_event,

    .event_handler_ctx = NULL

};

