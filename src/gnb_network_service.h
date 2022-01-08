#ifndef gnb_network_service_h
#define gnb_network_service_h


#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#include "gnb_buf.h"

#include "gnb_event.h"

#include "gnb_fixed_pool.h"

#include "gnb_address_type.h"

#include "gnb_log_type.h"


typedef struct _gnb_connection_t{

	int fd;

    #define TCP_CONNECT_INIT     0x0
    #define TCP_CONNECT_SUCCESS  0x1
    #define TCP_CONNECT_WAIT     0x2
    #define TCP_CONNECT_FAIL     0x3
    #define UDP_CHANNEL          0x4


    #define TCP_CONNECT_FINISH   0x5
    #define UDP_CHANNEL_FINISH   0x6
    int status;

	gnb_sockaddress_t local_sockaddress;
	gnb_sockaddress_t remote_sockaddress;

	// 创建该tcp连接时的 listen socket 句柄
	int listen_tcp_fd;

	uint64_t    keepalive_ts_sec;

	gnb_zbuf_t  *recv_zbuf;

	gnb_zbuf_t  *send_zbuf;

	gnb_event_t *event;

	uint64_t n_recv;
	uint64_t n_send;

	uint64_t n_recv_total;
	uint64_t n_send_total;


	//存放针对这个地址端口的服务的一些信息
	void *conn_data;

	void *udata;

}gnb_connection_t;



typedef struct _gnb_network_service_t gnb_network_service_t;

typedef int (*gnb_service_init_cb_t)(gnb_network_service_t *service);

typedef int (*gnb_service_listen_cb_t)(gnb_network_service_t *service);

typedef int (*gnb_service_accept_cb_t)(gnb_network_service_t *service, gnb_connection_t *conn);

typedef int (*gnb_service_connect_cb_t)(gnb_network_service_t *service, gnb_connection_t *conn);

typedef int (*gnb_service_recv_cb_t)(gnb_network_service_t *service, gnb_connection_t *conn);

typedef int (*gnb_service_send_cb_t)(gnb_network_service_t *service, gnb_connection_t *conn);

typedef int (*gnb_service_close_cb_t)(gnb_network_service_t *service, gnb_connection_t *conn);

typedef int (*gnb_service_idle_cb_t)(gnb_network_service_t *service);




typedef struct _gnb_conn_array_t{

	size_t size;

	gnb_connection_t conn[0];

}gnb_conn_array_t;



typedef struct _gnb_network_service_t{

    gnb_heap_t *heap;

    gnb_log_ctx_t *log;

    gnb_fixed_pool_t *event_fixed_pool;
    
	size_t recv_zbuf_size;
	size_t send_zbuf_size;

	gnb_event_cmd *event_cmd;

	gnb_conn_array_t *conn_array;

	#define m_conn0 conn_array->conn[0]
	#define m_conn1 conn_array->conn[1]
	#define m_conn2 conn_array->conn[2]

	#define m_local_sockaddress0 conn_array->conn[0].local_sockaddress
	#define m_local_sockaddress1 conn_array->conn[1].local_sockaddress
	#define m_local_sockaddress2 conn_array->conn[2].local_sockaddress
	#define m_local_sockaddress3 conn_array->conn[3].local_sockaddress
	#define m_local_sockaddress4 conn_array->conn[4].local_sockaddress

	#define m_remote_sockaddress0 conn_array->conn[0].remote_sockaddress
	#define m_remote_sockaddress1 conn_array->conn[1].remote_sockaddress
	#define m_remote_sockaddress2 conn_array->conn[2].remote_sockaddress
	#define m_remote_sockaddress3 conn_array->conn[3].remote_sockaddress
	#define m_remote_sockaddress4 conn_array->conn[4].remote_sockaddress

    #define m_conn0_fd conn_array->conn[0].fd
	#define m_conn1_fd conn_array->conn[1].fd
	#define m_conn2_fd conn_array->conn[2].fd
	#define m_conn3_fd conn_array->conn[3].fd
	#define m_conn4_fd conn_array->conn[4].fd

	gnb_service_init_cb_t    init_cb;
	gnb_service_listen_cb_t  listen_cb;
	gnb_service_accept_cb_t  accept_cb;
	gnb_service_connect_cb_t connect_cb;
	gnb_service_recv_cb_t    recv_cb;

	//告知发送的情况，缓冲区剩余字节或全部发送成功
	gnb_service_send_cb_t    send_cb;
	gnb_service_close_cb_t   close_cb;
	gnb_service_idle_cb_t    idle_cb;


	struct timeval now_timeval;
	uint64_t now_time_sec;
	uint64_t now_time_usec;

	void *ctx;

	void *service_conf;

}gnb_network_service_t;



gnb_network_service_t* gnb_network_service_create(gnb_network_service_t *service_mod, gnb_log_ctx_t *log, size_t max_event);

int gnb_network_service_init(gnb_network_service_t *service, void *service_conf);

void gnb_network_service_destroy(gnb_network_service_t *service);

int gnb_network_service_listen(gnb_network_service_t *service);

//考虑增加一个这样的 callback 机制为 client 做一些初始化连接的工作
int gnb_network_service_init_channel(gnb_network_service_t *service);

void gnb_network_service_loop(gnb_network_service_t *service);


#define GNB_SERVICE_NOTIFY_SEND(service, conn) service->event_cmd->set_event(service->event_cmd, conn->event, GNB_EVENT_OP_ENABLE, GNB_EVENT_TYPE_WRITE)
#define GNB_SERVICE_DEL_EVENT(service, event) service->event_cmd->del_event(service->event_cmd, event)


gnb_connection_t* gnb_connection_create(gnb_network_service_t *service);
void gnb_connection_release(gnb_network_service_t *service, gnb_connection_t *conn);

void gnb_connection_close(gnb_network_service_t *service, gnb_connection_t *conn);

//gnb_network_service_tcp_connect
int gnb_network_service_connect(gnb_network_service_t *service, gnb_connection_t *conn);
//gnb_network_service_udp_connect
int gnb_network_service_udp_channel(gnb_network_service_t *service, gnb_connection_t *conn);

int gnb_network_service_udp_send(gnb_network_service_t *service, gnb_connection_t *conn);


#define GNB_EVENT_PAYLOAD_TYPE_UDPLOG    0x45


#define GNB_LOG_ID_EVENT_CORE          0


#endif

