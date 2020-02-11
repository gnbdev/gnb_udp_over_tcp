#include <stdint.h>
#include <stdlib.h>

#include "gnb_network_service.h"
#include "gnb_udp_over_tcp.h"

#include "gnb_address.h"
#include "gnb_alloc.h"
#include "gnb_lru32.h"
#include "gnb_buf.h"
#include "gnb_payload16.h"

#define UTO_MAX_TCP_PAYLOAD_SIZE 4096

typedef struct _uot_channel_t{

	gnb_connection_t *tcp_conn;

	gnb_connection_t *udp_conn;

	gnb_network_service_t *service;

}uot_channel_t;



typedef struct _uot_table_t{

	gnb_sockaddress_t remote_udp_sockaddress;

}uot_table_t;



typedef struct _uot_tcp_service_ctx_t{

    gnb_heap_t *heap;

    gnb_lru32_t *channel_lru;


	gnb_payload16_ctx_t  *gnb_payload16_ctx;

}uot_tcp_service_ctx_t;



static int service_init_cb(gnb_network_service_t *service){

	uot_tcp_service_ctx_t *service_ctx = (uot_tcp_service_ctx_t *)gnb_heap_alloc(service->heap, sizeof(uot_tcp_service_ctx_t));

    service_ctx->heap = service->heap;

    service_ctx->gnb_payload16_ctx = gnb_payload16_ctx_init(UTO_MAX_TCP_PAYLOAD_SIZE);

    service_ctx->channel_lru = gnb_lru32_create(service_ctx->heap, 1024, sizeof(uot_channel_t));

	service->ctx = service_ctx;
	service->socket_array = (gnb_service_socket_array_t *)gnb_heap_alloc(service->heap, sizeof(gnb_service_socket_array_t) * sizeof(gnb_service_socket_t) * 2 );
	service->socket_array->size = 2;

	return 0;

}


static int service_listen_cb(gnb_network_service_t *service){

	uot_tcp_service_ctx_t *service_ctx = (uot_tcp_service_ctx_t *)service->ctx;

	udp_over_tcp_service_conf_t *udp_over_tcp_service_conf = (udp_over_tcp_service_conf_t *)service->service_conf;

	gnb_set_sockaddress4(&service->m_sockaddress0, GNB_PROTOCOL_TCP, "0.0.0.0", udp_over_tcp_service_conf->listen_port);

	uot_table_t *uot_table0 = gnb_heap_alloc(service->heap, sizeof(uot_table_t));

	gnb_set_sockaddress4(&uot_table0->remote_udp_sockaddress, GNB_PROTOCOL_UDP, udp_over_tcp_service_conf->des_udp_address, udp_over_tcp_service_conf->des_udp_port);

	service->m_conn0.conn_data = uot_table0;

	return 0;

}


static int service_accept_cb(gnb_network_service_t *service, gnb_connection_t *conn){

	uot_tcp_service_ctx_t *service_ctx = (uot_tcp_service_ctx_t *)service->ctx;

	uot_table_t *uot_table = (uot_table_t *)conn->conn_data;

	uot_channel_t *channel = gnb_heap_alloc(service->heap, sizeof(uot_channel_t));

	channel->tcp_conn = conn;

	channel->udp_conn = gnb_connection_create(service);

	channel->udp_conn->remote_sockaddress = uot_table->remote_udp_sockaddress;

	gnb_network_service_udp_channel(service, channel->udp_conn);

	channel->tcp_conn->udata = channel;
	channel->udp_conn->udata = channel;
	channel->service = service;

	return 0;

}


static void udp_to_tcp(gnb_network_service_t *service, uot_channel_t *channel){

	gnb_connection_t *udp_conn = channel->udp_conn;
	gnb_connection_t *tcp_conn = channel->tcp_conn;

	int recv_len;

	recv_len = (GNB_BUF_LEN(udp_conn->recv_zbuf));


	if ( recv_len > GNB_BUF_SIZE(tcp_conn->send_zbuf) ){
		//drop
		GNB_BUF_RESET(udp_conn->recv_zbuf);
		return;
	}

	if ( recv_len > GNB_BUF_REMAIN(tcp_conn->send_zbuf) ){
		//drop
		GNB_BUF_RESET(udp_conn->recv_zbuf);
		printf("tcp side tcp buffer is FULL!!\n");
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



static int payload16_handle_callback(gnb_payload16_t *payload16, void *ctx) {

	uot_channel_t *channel = (uot_channel_t *)ctx;

	gnb_connection_t *tcp_conn = channel->tcp_conn;
	gnb_connection_t *udp_conn = channel->udp_conn;

	gnb_network_service_t *service = (gnb_network_service_t *)channel->service;

	uot_tcp_service_ctx_t *service_ctx = (uot_tcp_service_ctx_t *)service->ctx;


	uint32_t payload_size     = gnb_payload16_size(payload16);
	uint32_t payload_data_len = gnb_payload16_data_len(payload16);

	GNB_BUF_RESET(udp_conn->send_zbuf);

	if ( payload_size > GNB_BUF_REMAIN(udp_conn->send_zbuf) ){
		//Drop the udp payload
		return 1;
	}

	memcpy(udp_conn->send_zbuf->pos, payload16->data, gnb_payload16_data_len(payload16));

	udp_conn->send_zbuf->las += gnb_payload16_data_len(payload16);

	ssize_t n_send = gnb_network_service_udp_send(service, udp_conn);

	if( -1 == n_send ) {
		GNB_SERVICE_NOTIFY_SEND(service, udp_conn);
	}

	if ( n_send > 0 ){
		GNB_BUF_RESET(udp_conn->send_zbuf);
	}

    return 0;

}



static void tcp_to_udp(gnb_network_service_t *service, uot_channel_t *channel){

	uot_tcp_service_ctx_t *service_ctx = (uot_tcp_service_ctx_t *)service->ctx;

	gnb_connection_t *tcp_conn = channel->tcp_conn;
	gnb_connection_t *udp_conn = channel->udp_conn;

	int recv_len;

	recv_len = (GNB_BUF_LEN(tcp_conn->recv_zbuf));

	service_ctx->gnb_payload16_ctx->udata = channel;

	int ret;

	ret = gnb_payload16_handle(tcp_conn->recv_zbuf->pos, recv_len, service_ctx->gnb_payload16_ctx, payload16_handle_callback);

	if ( ret < 0 ){
		tcp_conn->status = TCP_CONNECT_FINISH;
		return;
	}


	GNB_BUF_RESET(tcp_conn->recv_zbuf);

}



static int service_recv_cb(gnb_network_service_t *service, gnb_connection_t *conn){

	uot_tcp_service_ctx_t *service_ctx = (uot_tcp_service_ctx_t *)service->ctx;

	uot_channel_t *channel = NULL;

	int recv_len;

	channel = (uot_channel_t *)conn->udata;

	if (NULL==channel){
		return 0;
	}

	if ( GNB_EVENT_FD_UDP4_SOCKET == conn->event->fd_type ){
		udp_to_tcp(service, channel);
		return 0;
	}


	if ( GNB_EVENT_FD_TCPV4_CONNECT == conn->event->fd_type ){
		tcp_to_udp(service, channel);
		return 0;
	}


	return 0;

}



static int service_send_cb(gnb_network_service_t *service, gnb_connection_t *conn){

	GNB_BUF_RESET(conn->send_zbuf);

	return 0;

}



static int service_close_cb(gnb_network_service_t *service, gnb_connection_t *conn){

	uot_tcp_service_ctx_t *service_ctx = (uot_tcp_service_ctx_t *)service->ctx;

	uot_channel_t *channel = NULL;

	channel = (uot_channel_t *)conn->udata;

	if (NULL==channel){
		return 0;
	}

	gnb_connection_close(service, channel->udp_conn);

	gnb_heap_free(service->heap,channel);

	return 0;

}


static int service_idle_cb(gnb_network_service_t *service){

	return 0;

}


gnb_network_service_t uot_tcp_service_mod = {

	.recv_zbuf_size = 2048,
	.send_zbuf_size = 2048,

	.event_cmd      = NULL,

	.socket_array   = NULL,

	.init_cb        = service_init_cb,

	.listen_cb      = service_listen_cb,

	.accept_cb      = service_accept_cb,

	.connect_cb     = NULL,

	.recv_cb        = service_recv_cb,

	.send_cb        = service_send_cb,

	.close_cb       = service_close_cb,

	.idle_cb        = service_idle_cb,

	.ctx            = NULL

};

