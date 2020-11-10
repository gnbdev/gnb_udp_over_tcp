#include <stdlib.h>

#include "gnb_network_service.h"
#include "gnb_udp_over_tcp.h"

#include "gnb_address.h"
#include "gnb_alloc.h"
#include "gnb_hash32.h"
#include "gnb_buf.h"
#include "gnb_payload16.h"
#include "gnb_address.h"
#include "gnb_log.h"


#define DEFAULT_TCP_RECONNECT_INTERVAL_TIME_SEC 1


#define UTO_MAX_TCP_PAYLOAD_SIZE 4096


typedef struct _uot_udp_service_session_t{

	gnb_connection_t *tcp_conn;
	gnb_connection_t *udp_conn;
	uint64_t last_reconnect_time_sec;

}uot_udp_service_session_t;


typedef struct _uot_udp_service_ctx_t{

    gnb_heap_t *heap;

    uot_udp_service_session_t *session;

	gnb_payload16_ctx_t  *gnb_payload16_ctx;

}uot_udp_service_ctx_t;



static int start_tcp_connect(gnb_network_service_t *service){

	uot_udp_service_ctx_t *service_ctx = (uot_udp_service_ctx_t *)service->ctx;

	udp_over_tcp_service_conf_t *udp_over_tcp_service_conf = (udp_over_tcp_service_conf_t *)service->service_conf;

	int rc;

	gnb_connection_t *conn = gnb_connection_create(service);

	gnb_set_sockaddress4(&conn->remote_sockaddress, GNB_PROTOCOL_TCP, udp_over_tcp_service_conf->tcp_address, udp_over_tcp_service_conf->tcp_port);

    conn->udata = service_ctx->session;

    service_ctx->session->tcp_conn = conn;

    rc = gnb_network_service_connect(service, conn);

    GNB_STD1(service->log, GNB_LOG_ID_UOT, "connect %s rc=%d\n", GNB_SOCKETADDRSTR1(&conn->remote_sockaddress), rc);

    return rc;

}



static int service_init_cb(gnb_network_service_t *service){

	uot_udp_service_ctx_t *service_ctx = (uot_udp_service_ctx_t *)gnb_heap_alloc(service->heap, sizeof(uot_udp_service_ctx_t));

    service_ctx->heap = service->heap;

    service_ctx->gnb_payload16_ctx = gnb_payload16_ctx_init(UTO_MAX_TCP_PAYLOAD_SIZE);
    service_ctx->gnb_payload16_ctx->udata = service;

	service->ctx = service_ctx;

	service_ctx->session = (uot_udp_service_session_t *)gnb_heap_alloc(service->heap,sizeof(uot_udp_service_session_t));

	memset(service_ctx->session,0,sizeof(uot_udp_service_session_t));

	return 0;

}



static int service_listen_cb(gnb_network_service_t *service){

	udp_over_tcp_service_conf_t *udp_over_tcp_service_conf = (udp_over_tcp_service_conf_t *)service->service_conf;

	service->socket_array = (gnb_service_socket_array_t *)gnb_heap_alloc(service->heap, sizeof(gnb_service_socket_array_t) * sizeof(gnb_service_socket_t) * 2 );

	service->socket_array->size = 2;

	uot_udp_service_ctx_t *service_ctx = (uot_udp_service_ctx_t *)service->ctx;

	gnb_set_sockaddress4(&service->m_sockaddress0, GNB_PROTOCOL_UDP, "0.0.0.0", udp_over_tcp_service_conf->listen_port);

	start_tcp_connect(service);

	return 0;

}


static int service_connect_cb(gnb_network_service_t *service, gnb_connection_t *conn){

	return 0;

}



static int payload16_handle_callback(gnb_payload16_t *payload16, void *ctx) {

    gnb_network_service_t *service = (gnb_network_service_t *)ctx;

    uot_udp_service_ctx_t *service_ctx = (uot_udp_service_ctx_t *)service->ctx;

    uot_udp_service_session_t *session = service_ctx->session;





    //判断 payload 的类型，如果是 keepalive 要回应,并记录到 keepalive_ts_sec






    gnb_connection_t *udp_conn = session->udp_conn;

    uint32_t frame_size = gnb_payload16_size(payload16);

    int recv_len = gnb_payload16_data_len(payload16);

    if ( recv_len > GNB_BUF_REMAIN(udp_conn->send_zbuf) ){
    	return 0;
    }

    memcpy(udp_conn->send_zbuf->las, payload16->data, recv_len);

    udp_conn->send_zbuf->las += recv_len;

    ssize_t n_send = gnb_network_service_udp_send(service, udp_conn);

	if( -1 == n_send ) {
		GNB_SERVICE_NOTIFY_SEND(service, udp_conn);
	}

	if ( n_send > 0 ){
		GNB_BUF_RESET(udp_conn->send_zbuf);
	}

    return 0;

}




/*
改名为 handle_tcp，并且要 处理 tcp keepalive
*/

static void tcp_to_udp(gnb_network_service_t *service, gnb_connection_t *tcp_conn, gnb_connection_t *udp_conn){

	int recv_len;

	recv_len = (GNB_BUF_LEN(tcp_conn->recv_zbuf));

	int rc;

	uot_udp_service_ctx_t *service_ctx = (uot_udp_service_ctx_t *)service->ctx;

	rc = gnb_payload16_handle(tcp_conn->recv_zbuf->pos, recv_len, service_ctx->gnb_payload16_ctx, payload16_handle_callback);

	if ( rc < 0 ){

		tcp_conn->status = TCP_CONNECT_FINISH;

		GNB_STD1(service->log, GNB_LOG_ID_UOT, "handle payload error rc=%d %s\n", rc, GNB_SOCKETADDRSTR1(&tcp_conn->remote_sockaddress));

		return;
	}

	GNB_BUF_RESET(tcp_conn->recv_zbuf);

}

/*
改名为 handle_udp
*/
static void udp_to_tcp(gnb_network_service_t *service, gnb_connection_t *udp_conn, gnb_connection_t *tcp_conn){

	int recv_len;

	recv_len = (GNB_BUF_LEN(udp_conn->recv_zbuf));

	if ( recv_len > GNB_BUF_SIZE(tcp_conn->send_zbuf) ){
		GNB_STD1(service->log, GNB_LOG_ID_UOT, "udp side send buffer is small conn[%s]\n", GNB_SOCKETADDRSTR1(&tcp_conn->remote_sockaddress));
		GNB_BUF_RESET(udp_conn->recv_zbuf);
		return;
	}

	if ( recv_len > GNB_BUF_REMAIN(tcp_conn->send_zbuf) ){
		GNB_BUF_RESET(udp_conn->recv_zbuf);
		GNB_STD1(service->log, GNB_LOG_ID_UOT, "udp side tcp buffer is FULL! conn[%s]\n", GNB_SOCKETADDRSTR1(&tcp_conn->remote_sockaddress));
		goto finish;
	}

	gnb_payload16_t  *payload16;

	payload16 = (gnb_payload16_t  *)tcp_conn->send_zbuf->las;

	gnb_payload16_set_data_len(payload16,recv_len);

	payload16->type = 'u';

	memcpy(payload16->data, udp_conn->recv_zbuf->pos, recv_len);

	tcp_conn->send_zbuf->las += gnb_payload16_size(payload16);

finish:

	GNB_BUF_RESET(udp_conn->recv_zbuf);

	GNB_SERVICE_NOTIFY_SEND(service, tcp_conn);

}


static int service_recv_cb(gnb_network_service_t *service, gnb_connection_t *conn){

	uot_udp_service_ctx_t *service_ctx = (uot_udp_service_ctx_t *)service->ctx;

	uot_udp_service_session_t *session = service_ctx->session;

	gnb_connection_t *tcp_conn;
	gnb_connection_t *udp_conn;

	int recv_len;

	if ( GNB_EVENT_FD_UDP4_SOCKET == conn->event->fd_type ){

		session->udp_conn = conn;

		tcp_conn = session->tcp_conn;
		udp_conn = session->udp_conn;

		if (NULL==tcp_conn){
			GNB_BUF_RESET(udp_conn->recv_zbuf);
			GNB_STD1(service->log, GNB_LOG_ID_UOT, "udp side tcp connect not init remote address[%s]\n", GNB_SOCKETADDRSTR1(&tcp_conn->remote_sockaddress));
			return 0;
		}

		if (TCP_CONNECT_SUCCESS != tcp_conn->status){
			GNB_STD1(service->log, GNB_LOG_ID_UOT, "udp side tcp connect not ready remote address[%s]\n", GNB_SOCKETADDRSTR1(&tcp_conn->remote_sockaddress));
			return 0;
		}

		udp_to_tcp(service, udp_conn, tcp_conn);

		return 0;

	}


	if ( GNB_EVENT_FD_TCPV4_CONNECT == conn->event->fd_type ){

		tcp_conn = conn;
		udp_conn = session->udp_conn;

		if(NULL==session->udp_conn){
			GNB_STD1(service->log, GNB_LOG_ID_UOT, "tcp side udp not ready remote address[%s] \n", GNB_SOCKETADDRSTR1(&tcp_conn->remote_sockaddress));
			GNB_BUF_RESET(tcp_conn->recv_zbuf);
			return 0;
		}


		tcp_to_udp(service, tcp_conn, session->udp_conn);

		return 0;
	}


	return 0;

}


static int service_send_cb(gnb_network_service_t *service, gnb_connection_t *conn){

	GNB_BUF_RESET(conn->send_zbuf);

	return 0;

}


static int service_close_cb(gnb_network_service_t *service, gnb_connection_t *conn){

	uot_udp_service_ctx_t *service_ctx = (uot_udp_service_ctx_t *)service->ctx;

	uot_udp_service_session_t *session = service_ctx->session;

	session->tcp_conn = NULL;

	return 0;

}


static int service_idle_cb(gnb_network_service_t *service){

	uot_udp_service_ctx_t *service_ctx = (uot_udp_service_ctx_t *)service->ctx;

	uot_udp_service_session_t *session = service_ctx->session;

	if( (service->now_time_sec - session->last_reconnect_time_sec) > DEFAULT_TCP_RECONNECT_INTERVAL_TIME_SEC ){

		if( NULL==session->tcp_conn ) {
			start_tcp_connect(service);
			session->last_reconnect_time_sec = service->now_time_sec;
		}

	}

#if 0
	if( NULL==session->tcp_conn ) {
		return 0;
	}


	// add keepalive handle

#endif

	return 0;

}


gnb_network_service_t uot_udp_service_mod = {

	.recv_zbuf_size = 2048,
	.send_zbuf_size = 2048,

	.event_cmd      = NULL,

	.socket_array   = NULL,

	.init_cb        = service_init_cb,

	.listen_cb      = service_listen_cb,

	.accept_cb      = NULL,

	.connect_cb     = service_connect_cb,

	.recv_cb        = service_recv_cb,

	.send_cb        = service_send_cb,

	.close_cb       = service_close_cb,

	.idle_cb        = service_idle_cb,

	.ctx            = NULL

};

