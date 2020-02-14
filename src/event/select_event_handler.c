#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gnb_platform.h"


#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <unistd.h>

#ifdef __UNIX_LIKE_OS__
#include <sys/select.h>
#endif

#include <errno.h>

#include "gnb_event.h"

typedef struct _select_handler_ctx_t{

	int maxfd;

	fd_set readfd_set;
	fd_set writefd_set;
	fd_set exceptfd_set;
	gnb_event_array_t *event_array;

}select_handler_ctx_t;


static int init_event(gnb_event_cmd *event_cmd, size_t max_event) {

	select_handler_ctx_t *handler_ctx = malloc(sizeof(select_handler_ctx_t));

	memset(handler_ctx,0,sizeof(select_handler_ctx_t));

	handler_ctx->maxfd = -1;

	FD_ZERO(&handler_ctx->readfd_set);
	FD_ZERO(&handler_ctx->writefd_set);
	FD_ZERO(&handler_ctx->exceptfd_set);

	handler_ctx->event_array = (gnb_event_array_t *)malloc( sizeof(gnb_event_array_t) + sizeof(gnb_event_t) * max_event );

	memset(handler_ctx->event_array, 0, sizeof(gnb_event_array_t) + sizeof(gnb_event_t) * max_event);

	handler_ctx->event_array->size = max_event;

	event_cmd->event_handler_ctx = handler_ctx;

	return 0;

}



static int add_event (gnb_event_cmd *event_cmd, gnb_event_t *ev, int ev_type) {

	select_handler_ctx_t *handler_ctx = (select_handler_ctx_t *)event_cmd->event_handler_ctx;

	if ( handler_ctx->event_array->nevent >= handler_ctx->event_array->size ){

		return -1;
	}

	if( ev->fd > handler_ctx->maxfd ){

		handler_ctx->maxfd = ev->fd;
	}

    /* ************************************ */
	handler_ctx->event_array->list[ handler_ctx->event_array->nevent ] = ev;
	ev->index = handler_ctx->event_array->nevent;
	handler_ctx->event_array->nevent++;
    /* ************************************ */
    
	if ( GNB_EVENT_TYPE_READ & ev_type){
		FD_SET(ev->fd, &handler_ctx->readfd_set);
	}

	if ( GNB_EVENT_TYPE_WRITE & ev_type){
		FD_SET(ev->fd, &handler_ctx->writefd_set);
	}

	FD_SET(ev->fd, &handler_ctx->exceptfd_set);

	//如果ev->uevent为NULL，目前的机制将无法删除事件
	ev->uevent = &ev->fd;

	return 0;

}



static int set_event ( gnb_event_cmd *event_cmd, gnb_event_t *ev, int op, int ev_type ) {

	select_handler_ctx_t *handler_ctx = (select_handler_ctx_t *)event_cmd->event_handler_ctx;

	switch (op) {

	case GNB_EVENT_OP_ENABLE:

		if( GNB_EVENT_TYPE_WRITE & ev_type ){
			FD_SET(ev->fd, &handler_ctx->writefd_set);
		}

		if( GNB_EVENT_TYPE_READ & ev_type ){
			FD_SET(ev->fd, &handler_ctx->readfd_set);
		}

		break;

	case GNB_EVENT_OP_DISABLE:

		if ( GNB_EVENT_TYPE_WRITE & ev_type ) {
			FD_CLR(ev->fd, &handler_ctx->writefd_set);
		}

		if ( GNB_EVENT_TYPE_READ & ev_type ) {
			FD_CLR(ev->fd, &handler_ctx->readfd_set);
		}

		break;

	default:
		break;

	};

	return 0;

}



static int del_event ( gnb_event_cmd *event_cmd, gnb_event_t *ev ) {

	select_handler_ctx_t *handler_ctx = (select_handler_ctx_t *)event_cmd->event_handler_ctx;

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

    FD_CLR(ev->fd, &handler_ctx->readfd_set);
    FD_CLR(ev->fd, &handler_ctx->writefd_set);
    FD_CLR(ev->fd, &handler_ctx->exceptfd_set);

	return 0;
}



static int get_event (gnb_event_cmd *event_cmd, gnb_event_t **ev_lst, int *nevents) {

	select_handler_ctx_t *handler_ctx = (select_handler_ctx_t *)event_cmd->event_handler_ctx;

	fd_set readfds;
	fd_set writefds;
	fd_set exceptfds;
	int n_ready;

	int idx = 0;

	struct timeval timeout;
	timeout.tv_sec  = 0l;
	timeout.tv_usec = 10000l;

	readfds  = handler_ctx->readfd_set;
	writefds = handler_ctx->writefd_set;
	exceptfds = handler_ctx->exceptfd_set;

	n_ready = select( handler_ctx->maxfd + 1, &readfds, &writefds, &exceptfds, &timeout );

	if ( -1 == n_ready ) {

		if ( EINTR == errno ) {
			goto finish;
		}else{
			goto finish;
		}

    }

	if ( 0 == n_ready ) {
		goto finish;
	}

	int i;

	int ev_type;

	for ( i = 0; i < handler_ctx->event_array->nevent; i++ ) {

		ev_type = GNB_EVENT_TYPE_NONE;

		if ( FD_ISSET(handler_ctx->event_array->list[i]->fd, &readfds) ){
			ev_type |= GNB_EVENT_TYPE_READ;
		}

		if ( FD_ISSET(handler_ctx->event_array->list[i]->fd, &writefds) ){
			ev_type |= GNB_EVENT_TYPE_WRITE;
		}

		if ( FD_ISSET(handler_ctx->event_array->list[i]->fd, &exceptfds) ){
			ev_type |= GNB_EVENT_TYPE_ERROR;
		}

		if ( GNB_EVENT_TYPE_NONE != ev_type ){
			ev_lst[idx] = handler_ctx->event_array->list[i];
			ev_lst[idx]->ev_type = ev_type;
			idx++;
		}

		if ( idx == n_ready ){
			break;
		}


	}

finish:

	*nevents = idx;

	return 0;

}

static int finish_event(gnb_event_cmd *event_cmd){
    
    select_handler_ctx_t *handler_ctx = (select_handler_ctx_t *)event_cmd->event_handler_ctx;

    free(handler_ctx->event_array);

    free(event_cmd->event_handler_ctx);
    
    return 0;
}

gnb_event_cmd select_event_cmd = {

	.mod_name = "select_event",

	.heap = NULL,

	.self = &select_event_cmd,

	.init_event = init_event,

	.add_event  = add_event,
	.set_event  = set_event,
	.del_event  = del_event,
	.get_event  = get_event,

	.finish_event = finish_event,

	.event_handler_ctx = NULL

};

