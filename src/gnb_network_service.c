#include <stdlib.h>

#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#define _POSIX

#include <winsock2.h>
#include <ws2tcpip.h>
#endif


#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


#include "gnb_platform.h"
#include "gnb_network_service.h"
#include "gnb_address.h"
#include "gnb_log.h"


static int do_close(gnb_network_service_t *service, gnb_event_t *event);


static void sync_time(gnb_network_service_t *service){

    gettimeofday(&service->now_timeval, NULL);

    service->now_time_sec  = (uint64_t)service->now_timeval.tv_sec;
    service->now_time_usec = (uint64_t)service->now_timeval.tv_sec *1000000 + service->now_timeval.tv_usec;

}


static void network_service_set_signal(){
    signal(SIGPIPE, SIG_IGN);
}


gnb_connection_t* gnb_connection_create(gnb_network_service_t *service){

    gnb_connection_t *conn = gnb_heap_alloc(service->heap, sizeof(gnb_connection_t));

    memset(conn,0,sizeof(gnb_connection_t));

    return conn;

}

void gnb_connection_release(gnb_network_service_t *service, gnb_connection_t *conn){

    if (NULL!=conn->recv_zbuf) {
        gnb_heap_free(service->heap,conn->recv_zbuf);
    }

    if (NULL!=conn->send_zbuf) {
        gnb_heap_free(service->heap,conn->send_zbuf);
    }

    gnb_heap_free(service->heap,conn);

}


void gnb_connection_close(gnb_network_service_t *service, gnb_connection_t *conn){

    int ret;
    
    gnb_event_t *event = conn->event;

    event->ev_type = GNB_EVENT_TYPE_FINISH;

    if ( NULL != event->uevent ) {
        service->event_cmd->del_event(service->event_cmd, event);
    }

    ret = gnb_fixed_pool_push(service->event_fixed_pool, (void *)event);

#if defined(__UNIX_LIKE_OS__)
    close(conn->fd);
#endif

#ifdef _WIN32
    closesocket(conn->fd);
#endif

    conn->status = TCP_CONNECT_INIT;

    GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "close connect %s\n", GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));

}



gnb_network_service_t* gnb_network_service_create(gnb_network_service_t *service_mod, gnb_log_ctx_t *log, size_t max_event){

#ifdef _WIN32
    WSADATA wsaData;
    int err;
    err = WSAStartup(MAKEWORD(2, 2), &wsaData );
#endif

    gnb_heap_t *heap = gnb_heap_create(max_event*32);

    gnb_network_service_t *service = (gnb_network_service_t *)gnb_heap_alloc(heap, sizeof(gnb_network_service_t));

    memset(service, 0, sizeof(gnb_network_service_t));

    memcpy(service, service_mod, sizeof(gnb_network_service_t));

    sync_time(service);

    service->heap = heap;

    service->log = log;

    service->event_fixed_pool = gnb_fixed_pool_create(service->heap, max_event, sizeof(gnb_event_t));
    
    service->event_cmd = gnb_create_event_cmd();

    service->event_cmd->heap = service->heap;
    
    service->event_cmd->init_event(service->event_cmd, max_event);

    return service;

}


int gnb_network_service_init(gnb_network_service_t *service, void *service_conf){

    network_service_set_signal();

    service->service_conf = service_conf;

    service->init_cb(service);

    return 0;
}



void gnb_network_service_destroy(gnb_network_service_t *service){

#ifdef _WIN32
    WSACleanup();
#endif

}



int gnb_network_service_listen(gnb_network_service_t *service){

    service->listen_cb(service);

    int rc;

    int i;

    int on;

    for ( i=0; i<service->conn_array->size; i++ ) {

        service->conn_array->conn[i].fd = socket(service->conn_array->conn[i].local_sockaddress.addr_type, service->conn_array->conn[i].local_sockaddress.protocol, 0);

        if ( -1 == service->conn_array->conn[i].fd ) {
            GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "socket error index=%d %s\n", i, GNB_SOCKETADDRSTR1(&service->conn_array->conn[i].local_sockaddress) );
        }

        on = 1;
        setsockopt( service->conn_array->conn[i].fd, SOL_SOCKET, SO_REUSEADDR,(char *)&on, sizeof(on) );

        #if defined(__UNIX_LIKE_OS__)
        on = 1;
        setsockopt( service->conn_array->conn[i].fd, SOL_SOCKET, SO_REUSEPORT,(char *)&on, sizeof(on) );
        #endif


        if ( AF_INET6 == service->conn_array->conn[i].local_sockaddress.addr_type ) {

            rc = bind(service->conn_array->conn[i].fd, (struct sockaddr *)&service->conn_array->conn[i].local_sockaddress.m_in6, sizeof(struct sockaddr_in6) );

            GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "bind6 rc=%d %s\n", rc, GNB_SOCKETADDRSTR1(&service->conn_array->conn[i].local_sockaddress) );

        } else if ( AF_INET == service->conn_array->conn[i].local_sockaddress.addr_type ) {

            rc = bind(service->conn_array->conn[i].fd, (struct sockaddr *)&service->conn_array->conn[i].local_sockaddress.m_in4, sizeof(struct sockaddr_in) );

            GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "bind rc=%d %s\n", rc, GNB_SOCKETADDRSTR1(&service->conn_array->conn[i].local_sockaddress) );

        }


        if ( -1 == rc ) {
            perror("bind");
            exit(1);
        }

        //不是 tcp 就跳过
        if ( SOCK_STREAM != service->conn_array->conn[i].local_sockaddress.protocol ) {
            continue;
        }

        /*
         将 listen 的 socket fd 设为O_NONBLOCK, 避免Server调用 accept 之前客户端发送 RST 断开导致Server阻塞在 accept
        */
        #if defined(__UNIX_LIKE_OS__)
        on = fcntl(service->conn_array->conn[i].fd, F_GETFL, NULL );
        fcntl(service->conn_array->conn[i].fd, F_SETFL, on | O_NONBLOCK);
        #endif

        #if defined(_WIN32)
        unsigned long argul = 1l;
        rc = ioctlsocket(service->conn_array->conn[i].fd, FIONBIO, &argul);
        #endif

        rc = listen( service->conn_array->conn[i].fd, 10 );

        if ( -1 == rc ) {
            perror("listen");
            exit(1);
        }


    }

    return 0;

}



int gnb_network_service_udp_send(gnb_network_service_t *service, gnb_connection_t *conn){

    gnb_event_t *event = conn->event;

    ssize_t n_send = 0;

    if ( GNB_EVENT_FD_UDP6_SOCKET == event->fd_type ) {

        n_send = sendto(conn->fd, (void *)conn->send_zbuf->pos, GNB_BUF_LEN(conn->send_zbuf), 0,
                        (struct sockaddr *)&conn->remote_sockaddress.m_in6, sizeof(struct sockaddr_in6));

        GNB_DEBUG2(service->log, GNB_LOG_ID_EVENT_CORE, "send6 n_send=%d %s\n", n_send, GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));

    } else if ( GNB_EVENT_FD_UDP4_SOCKET == event->fd_type ) {

        n_send = sendto(conn->fd, (void *)conn->send_zbuf->pos, GNB_BUF_LEN(conn->send_zbuf), 0,
                (struct sockaddr *)&conn->remote_sockaddress.m_in4, sizeof(struct sockaddr_in));

        GNB_DEBUG2(service->log, GNB_LOG_ID_EVENT_CORE, "send n_send=%d %s\n", n_send, GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));

    }


    if ( -1 != n_send ) {
        conn->send_zbuf->pos += n_send;
        conn->n_send = n_send;
        conn->n_send_total += n_send;
    }

    return n_send;

}



static gnb_connection_t * do_accept(gnb_network_service_t *service, gnb_event_t *event){

    int connect_fd;
    int ret;

    socklen_t socklen;

    struct sockaddr_in6     connect_addr6;
    struct sockaddr_in      connect_addr;

    gnb_event_t *add_event;

    gnb_connection_t *conn;

    if ( GNB_EVENT_FD_TCP6_LISTEN == event->fd_type ) {

        socklen = sizeof(struct sockaddr_in6);

        connect_fd = accept(event->fd, (struct sockaddr*)&connect_addr6, &socklen);

        if ( -1==connect_fd ) {
            perror("accept");
            return NULL;
        }

        add_event = gnb_fixed_pool_pop(service->event_fixed_pool);

        add_event->fd = connect_fd;
        add_event->fd_type = GNB_EVENT_FD_TCPV4_CONNECT;

        conn = gnb_connection_create(service);
        conn->fd = connect_fd;
        conn->status = TCP_CONNECT_SUCCESS;

        conn->remote_sockaddress.addr_type = AF_INET6;
        conn->remote_sockaddress.protocol = SOCK_STREAM;
        memcpy(&conn->remote_sockaddress.m_in6, &connect_addr6, socklen);
        conn->remote_sockaddress.socklen = socklen;

        conn->listen_tcp_fd = event->fd;


    } else if ( GNB_EVENT_FD_TCP4_LISTEN == event->fd_type ) {

        socklen = sizeof(struct sockaddr_in);

        connect_fd = accept(event->fd, (struct sockaddr*)&connect_addr, &socklen);

        if ( -1==connect_fd ) {
            perror("accept");
            return NULL;
        }

        add_event = gnb_fixed_pool_pop(service->event_fixed_pool);
        
        add_event->fd = connect_fd;
        add_event->fd_type = GNB_EVENT_FD_TCPV4_CONNECT;

        conn = gnb_connection_create(service);
        conn->fd = connect_fd;
        conn->status = TCP_CONNECT_SUCCESS;

        conn->remote_sockaddress.addr_type = AF_INET;
        conn->remote_sockaddress.protocol = SOCK_STREAM;
        memcpy(&conn->remote_sockaddress.m_in4, &connect_addr, socklen);
        conn->remote_sockaddress.socklen = socklen;

        conn->listen_tcp_fd = event->fd;

    }

    gnb_connection_t *listen_conn = (gnb_connection_t *)event->udata;

    conn->local_sockaddress = listen_conn->local_sockaddress;
    conn->conn_data = listen_conn->conn_data;
    conn->event = add_event;
    conn->recv_zbuf = gnb_zbuf_heap_alloc(service->heap, service->recv_zbuf_size);
    conn->send_zbuf = gnb_zbuf_heap_alloc(service->heap, service->send_zbuf_size);

    add_event->udata = conn;

    ret = service->accept_cb(service, conn);

    if ( 0 != ret ) {
        return conn;
    }

    service->event_cmd->add_event(service->event_cmd, add_event, GNB_EVENT_TYPE_READ);

    return conn;

}


static int do_connect(gnb_network_service_t *service, gnb_event_t *event){

    int ret;

    gnb_sockaddress_t sockaddress;
    
    if ( GNB_EVENT_FD_TCPV6_CONNECT == event->fd_type ) {
        
        sockaddress.socklen = sizeof(struct sockaddr_in6);
        ret = getpeername(event->fd, (struct sockaddr *)&sockaddress.m_in6, &sockaddress.socklen);
        
    } else if ( GNB_EVENT_FD_TCPV4_CONNECT == event->fd_type ) {
        
        sockaddress.socklen = sizeof(struct sockaddr_in);
        ret = getpeername(event->fd, (struct sockaddr *)&sockaddress.m_in4, &sockaddress.socklen);
        
    } else {
        do_close(service, event);
        return -1;
    }

    if ( 0 != ret ) {
        do_close(service, event);
        return -1;
    }

    gnb_connection_t *conn = (gnb_connection_t *)event->udata;

    if ( NULL == service->connect_cb ) {
        do_close(service, event);
        return 0;
    }

    GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "try to connect %s\n", GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));
    
    conn->recv_zbuf = gnb_zbuf_heap_alloc(service->heap, service->recv_zbuf_size);
    conn->send_zbuf = gnb_zbuf_heap_alloc(service->heap, service->send_zbuf_size);

    service->event_cmd->set_event(service->event_cmd, conn->event, GNB_EVENT_OP_DISABLE, GNB_EVENT_TYPE_WRITE);

    service->connect_cb(service,conn);

    return 0;

}



static int do_recv(gnb_network_service_t *service, gnb_event_t *event){

    ssize_t n_recv;

    gnb_connection_t *conn = (gnb_connection_t *)event->udata;

    int r = GNB_BUF_REMAIN(conn->recv_zbuf);

    n_recv = recv(event->fd, conn->recv_zbuf->las, GNB_BUF_REMAIN(conn->recv_zbuf), 0);

    if ( 0 == n_recv ) {
        event->ev_type  |= GNB_EVENT_TYPE_EOF;
        return n_recv;
    }

    if ( -1 == n_recv ) {
        perror("tcp recv");
        goto handle_error;
    }

    if ( n_recv > 0 ) {
        conn->recv_zbuf->las = conn->recv_zbuf->pos + n_recv;
        conn->n_recv = n_recv;
        conn->n_recv_total += n_recv;

        goto finish;
    }

handle_error:

    switch (errno) {

            case EAGAIN:
            case EINTR:
                return 0;

        default:
            event->ev_type |= GNB_EVENT_TYPE_ERROR;
            break;

    }

finish:

    service->recv_cb(service,conn);

    return n_recv;
}



static int do_send(gnb_network_service_t *service, gnb_event_t *event){

    gnb_connection_t *conn = (gnb_connection_t *)event->udata;

    ssize_t n_send;

    size_t buf_len = GNB_BUF_LEN(conn->send_zbuf);

    if ( 0 == buf_len ) {
        n_send = 0;
        goto finish;
    }

    n_send = send(conn->fd, conn->send_zbuf->pos, GNB_BUF_LEN(conn->send_zbuf), 0);

    if ( -1 == n_send ) {
        goto handle_error;
    }

    conn->send_zbuf->pos += n_send;
    conn->n_send = n_send;
    conn->n_send_total += n_send;

    goto finish;


handle_error:
    
    switch (errno) {

        case EAGAIN:
        case EINTR:
        case ENOBUFS:
            return 0;

        case ECONNRESET:
        case ENOTCONN:
        case ENETDOWN:
            event->ev_type |= GNB_EVENT_TYPE_EOF;
            break;

        default:
            event->ev_type |= GNB_EVENT_TYPE_ERROR;
            break;

    }


finish:

    service->event_cmd->set_event(service->event_cmd, conn->event, GNB_EVENT_OP_DISABLE, GNB_EVENT_TYPE_WRITE);

    service->send_cb(service,conn);
    
    return n_send;

}


static int do_close(gnb_network_service_t *service, gnb_event_t *event){

    gnb_connection_t *conn = (gnb_connection_t *)event->udata;

    gnb_connection_close(service,conn);

    service->close_cb(service,conn);

    return 0;

}


static int handle_tcp_event(gnb_network_service_t *service, gnb_event_t *event){

    gnb_connection_t *conn = (gnb_connection_t *)event->udata;
    gnb_connection_t *new_conn;

    int rc = 0;

    if ( GNB_EVENT_TYPE_FINISH == event->ev_type) {
        return 1;
    }

    if ( GNB_EVENT_FD_TCP6_LISTEN == event->fd_type || GNB_EVENT_FD_TCP4_LISTEN == event->fd_type ) {

        new_conn = do_accept(service, event);

        if ( NULL != new_conn ) {
            GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "accept  remote %s\n", GNB_SOCKETADDRSTR1(&new_conn->remote_sockaddress));
        } else {
            GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "accept false listen fd=\n",  conn->fd);
        }

        goto finish;

    }


    if ( (GNB_EVENT_FD_TCPV6_CONNECT == event->fd_type || GNB_EVENT_FD_TCPV4_CONNECT == event->fd_type) && TCP_CONNECT_WAIT == conn->status ) {
        conn->status = TCP_CONNECT_SUCCESS;
        rc = do_connect(service, event);
        GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "connect rc=%d %s\n", rc, GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));
        goto finish;
    }


    if( GNB_EVENT_TYPE_WRITE & event->ev_type ) {
        rc = do_send(service, event);
        GNB_LOG2(service->log, GNB_LOG_ID_EVENT_CORE, "send rc=%d %s\n", rc, GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));
    }


    if( GNB_EVENT_TYPE_READ & event->ev_type ) {
        rc = do_recv(service, event);
        GNB_LOG2(service->log, GNB_LOG_ID_EVENT_CORE, "recv rc=%d %s\n", rc, GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));
    }


    if ( GNB_EVENT_TYPE_EOF & event->ev_type ) {
        do_close(service, event);
        GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "GNB_EVENT_TYPE_EOF %s\n", GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));
        return 0;
    }

    if ( GNB_EVENT_TYPE_ERROR & event->ev_type ) {
        GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "GNB_EVENT_TYPE_ERROR %s\n", GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));
        do_close(service, event);
        return 0;
    }


finish:

    if (TCP_CONNECT_FINISH == conn->status) {
        GNB_LOG1(service->log, GNB_LOG_ID_EVENT_CORE, "TCP_CONNECT_FINISH %s\n", GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));
        do_close(service, event);
    }

    return rc;

}



static int do_udp_recv(gnb_network_service_t *service, gnb_event_t *event){

    gnb_connection_t *conn = (gnb_connection_t *)event->udata;

    ssize_t n_recv;

    if ( GNB_EVENT_FD_UDP6_SOCKET == event->fd_type ) {

        conn->remote_sockaddress.socklen = sizeof(struct sockaddr_in6);

        n_recv = recvfrom(conn->fd, (void *)conn->recv_zbuf->las, GNB_BUF_REMAIN(conn->recv_zbuf), 0, (struct sockaddr *)&conn->remote_sockaddress.addr.in6, &conn->remote_sockaddress.socklen);

    } else if ( GNB_EVENT_FD_UDP4_SOCKET == event->fd_type ) {

        conn->remote_sockaddress.socklen = sizeof(struct sockaddr_in);

        n_recv = recvfrom(conn->fd, (void *)conn->recv_zbuf->las, GNB_BUF_REMAIN(conn->recv_zbuf), 0, (struct sockaddr *)&conn->remote_sockaddress.addr.in, &conn->remote_sockaddress.socklen);

    } else {
        return -1;
    }

    GNB_DEBUG2(service->log, GNB_LOG_ID_EVENT_CORE, "recv n_recv=%d %s\n", n_recv, GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));

    if ( -1 == n_recv ) {
        return n_recv;
    }

    conn->recv_zbuf->las += n_recv;

    conn->n_recv += n_recv;

    service->recv_cb(service,conn);

    return n_recv;

}



static int do_udp_send(gnb_network_service_t *service, gnb_event_t *event){

    gnb_connection_t *conn = (gnb_connection_t *)event->udata;

    ssize_t n_send;

    if ( GNB_EVENT_FD_UDP6_SOCKET == event->fd_type ) {

        n_send = sendto(conn->fd, (void *)conn->send_zbuf->pos, GNB_BUF_LEN(conn->send_zbuf), 0,
                        (struct sockaddr *)&conn->remote_sockaddress.m_in6, sizeof(struct sockaddr_in6));

    } else if ( GNB_EVENT_FD_UDP4_SOCKET == event->fd_type ) {

        n_send = sendto(conn->fd, (void *)conn->send_zbuf->pos, GNB_BUF_LEN(conn->send_zbuf), 0,
                (struct sockaddr *)&conn->remote_sockaddress.m_in4, sizeof(struct sockaddr_in));

    }

    GNB_DEBUG2(service->log, GNB_LOG_ID_EVENT_CORE, "recv sendto=%d %s\n", n_send, GNB_SOCKETADDRSTR1(&conn->remote_sockaddress));

    if ( -1 != n_send ) {
        conn->send_zbuf->pos += n_send;
        conn->n_send = n_send;
        conn->n_send_total += n_send;
    }

    service->send_cb(service,conn);

    service->event_cmd->set_event(service->event_cmd, conn->event, GNB_EVENT_OP_DISABLE, GNB_EVENT_TYPE_WRITE);

    return 0;

}



static int handle_udp_event(gnb_network_service_t *service, gnb_event_t *event){

    if ( GNB_EVENT_TYPE_FINISH == event->ev_type) {
        return 1;
    }


    if ( GNB_EVENT_TYPE_WRITE & event->ev_type ) {
        do_udp_send(service, event);
    }

    if ( GNB_EVENT_TYPE_READ & event->ev_type ) {
        do_udp_recv(service, event);
    }

    return 0;

}


void gnb_network_service_loop(gnb_network_service_t *service){

    int ret;

    int i;

    gnb_event_t *add_event;

    gnb_event_t *gnb_events[MAX_GET_GNB_EVENT];

    int num_gnb_ev;
    
    sync_time(service);

    if ( NULL == service->conn_array ) {
            goto loop;
    }
    
    for ( i=0; i<service->conn_array->size; i++ ) {

        if ( SOCK_STREAM == service->conn_array->conn[i].local_sockaddress.protocol ) {

            add_event = gnb_fixed_pool_pop(service->event_fixed_pool);

            memset(add_event,0,sizeof(gnb_event_t));

            add_event->fd = service->conn_array->conn[i].fd;

            if ( AF_INET6 == service->conn_array->conn[i].local_sockaddress.addr_type ) {
                add_event->fd_type = GNB_EVENT_FD_TCP6_LISTEN;
            } else {
                add_event->fd_type = GNB_EVENT_FD_TCP4_LISTEN;
            }

            add_event->udata = &service->conn_array->conn[i];

            service->conn_array->conn[i].event = add_event;

            service->event_cmd->add_event(service->event_cmd, add_event, GNB_EVENT_TYPE_READ);

        }


        if ( SOCK_DGRAM == service->conn_array->conn[i].local_sockaddress.protocol ) {

            add_event = gnb_fixed_pool_pop(service->event_fixed_pool);

            memset(add_event,0,sizeof(gnb_event_t));

            add_event->fd = service->conn_array->conn[i].fd;

            if( AF_INET6 == service->conn_array->conn[i].local_sockaddress.addr_type ) {
                add_event->fd_type = GNB_EVENT_FD_UDP6_SOCKET;
            } else {
                add_event->fd_type = GNB_EVENT_FD_UDP4_SOCKET;
            }

            add_event->udata = &service->conn_array->conn[i];


            service->conn_array->conn[i].event = add_event;

            service->conn_array->conn[i].recv_zbuf = gnb_zbuf_heap_alloc(service->heap, service->recv_zbuf_size);
            service->conn_array->conn[i].send_zbuf = gnb_zbuf_heap_alloc(service->heap, service->send_zbuf_size);

            service->event_cmd->add_event(service->event_cmd, add_event, GNB_EVENT_TYPE_READ);

        }

    }


loop:


    do{

        sync_time(service);

        num_gnb_ev = MAX_GET_GNB_EVENT;

        ret = service->event_cmd->get_event(service->event_cmd, gnb_events, &num_gnb_ev);

        if (0 == num_gnb_ev) {
            goto handle_idle;
        }

        for ( i=0; i <num_gnb_ev; i++) {

            if ( GNB_EVENT_FD_UDP4_SOCKET == gnb_events[i]->fd_type || GNB_EVENT_FD_UDP6_SOCKET == gnb_events[i]->fd_type ) {
                handle_udp_event(service, gnb_events[i]);
            } else {
                handle_tcp_event(service, gnb_events[i]);
            }

        }

    handle_idle:

        service->idle_cb(service);

    }while(1);

}



int gnb_network_service_connect(gnb_network_service_t *service, gnb_connection_t *conn){

    int ret;

    int flags;

    gnb_event_t *add_event;
    
    conn->fd = socket(conn->remote_sockaddress.addr_type, SOCK_STREAM, 0);

    if ( -1 == conn->fd ) {
        return -1;
    }

    #if defined(__UNIX_LIKE_OS__)
    flags = fcntl(conn->fd, F_GETFL, NULL);
    fcntl(conn->fd, F_SETFL, flags|O_NONBLOCK);
    #endif


    #if defined(_WIN32)
    unsigned long argul = 1l;
    ret = ioctlsocket(conn->fd, FIONBIO, &argul);
    #endif


    if ( AF_INET6 == conn->remote_sockaddress.addr_type ) {
        ret = connect( conn->fd, (struct sockaddr *)&conn->remote_sockaddress.m_in6, sizeof(struct sockaddr_in6) );
    } else if ( AF_INET == conn->remote_sockaddress.addr_type ) {
        ret = connect( conn->fd, (struct sockaddr *)&conn->remote_sockaddress.m_in4, sizeof(struct sockaddr_in) );
    }

    if ( 0 == ret ) {
        conn->status = TCP_CONNECT_SUCCESS;
        conn->recv_zbuf = gnb_zbuf_heap_alloc(service->heap, service->recv_zbuf_size);
        conn->send_zbuf = gnb_zbuf_heap_alloc(service->heap, service->send_zbuf_size);

        goto setup_event;
    }

    #if defined(__UNIX_LIKE_OS__)
    if ( errno == EINPROGRESS ) {
        conn->status = TCP_CONNECT_WAIT;
        goto setup_event;
    } else {
        close ( conn->fd );
        conn->status = TCP_CONNECT_FAIL;
        return -1;
    }

    #endif

    #if defined(_WIN32)
    if ( (WSAGetLastError() == WSAEWOULDBLOCK) ) {
        conn->status = TCP_CONNECT_WAIT;
        goto setup_event;

    } else {
        closesocket(conn->fd);
        conn->status = TCP_CONNECT_FAIL;
        return -1;
    }
    #endif



setup_event:

    add_event = gnb_fixed_pool_pop(service->event_fixed_pool);

    add_event->fd = conn->fd;

    if ( AF_INET6 == conn->remote_sockaddress.addr_type ) {
         add_event->fd_type = GNB_EVENT_FD_TCPV6_CONNECT;
    } else if( AF_INET == conn->remote_sockaddress.addr_type ) {
         add_event->fd_type = GNB_EVENT_FD_TCPV4_CONNECT;
    }

    conn->event = add_event;

    add_event->udata = conn;

    service->event_cmd->add_event(service->event_cmd, add_event, GNB_EVENT_TYPE_READ|GNB_EVENT_TYPE_WRITE);

finish:

    return 0;

}


int gnb_network_service_udp_channel(gnb_network_service_t *service, gnb_connection_t *conn){

    int ret;

    int flags;

    gnb_event_t *add_event;

    conn->fd = socket(conn->remote_sockaddress.addr_type, SOCK_DGRAM, 0);

    if ( -1 == conn->fd ) {
        return -1;
    }


    #if defined(__UNIX_LIKE_OS__)
    flags = fcntl(conn->fd, F_GETFL, NULL);
    fcntl(conn->fd, F_SETFL, flags|O_NONBLOCK);
    #endif


    #if defined(_WIN32)
    unsigned long argul = 1l;
    ret = ioctlsocket(conn->fd, FIONBIO, &argul);
    #endif


    conn->status = UDP_CHANNEL;
    conn->recv_zbuf = gnb_zbuf_heap_alloc(service->heap, service->recv_zbuf_size);
    conn->send_zbuf = gnb_zbuf_heap_alloc(service->heap, service->send_zbuf_size);

    add_event = gnb_fixed_pool_pop(service->event_fixed_pool);

    add_event->fd = conn->fd;

    if ( AF_INET6 == conn->remote_sockaddress.addr_type ) {
         add_event->fd_type = GNB_EVENT_FD_UDP6_SOCKET;
    } else if ( AF_INET == conn->remote_sockaddress.addr_type ) {
         add_event->fd_type = GNB_EVENT_FD_UDP4_SOCKET;
    }

    conn->event = add_event;

    add_event->udata = conn;

    service->event_cmd->add_event(service->event_cmd, add_event, GNB_EVENT_TYPE_READ);

finish:

    return 0;

}

