#ifndef gnb_udp_over_tcp_h
#define gnb_udp_over_tcp_h

#include <stdint.h>

#include "gnb_payload16.h"

#define UTO_MAX_TCP_PAYLOAD_SIZE 4096

#define GNB_UOT_KEEPALIVE_TIMEOUT_SEC       15

typedef struct _udp_over_tcp_service_conf_t{

	int tcp;
	int udp;

	uint16_t  listen_port;

	char     *tcp_address;
	uint16_t  tcp_port;

    char     *des_udp_address;
    uint16_t  des_udp_port;

}udp_over_tcp_service_conf_t;

#define GNB_LOG_ID_UOT                   1


#define GNB_PAYLOAD_TYPE_TCP_KEEPALIVE   'k'
#define GNB_PAYLOAD_SUB_TYPE_TCP_PING    'r'
#define GNB_PAYLOAD_SUB_TYPE_TCP_PONG    'p'

#define GNB_PAYLOAD_TYPE_UDP_OVER_TCP    'o'
#define GNB_PAYLOAD_SUB_TYPE_UDP_TO_TCP  'u'
#define GNB_PAYLOAD_SUB_TYPE_TCP_TO_UDP  't'


#pragma pack(push, 1)

typedef struct _uot_keepalive_frame_t {

	uint64_t src_ts_usec;

	unsigned char text[16];

}__attribute__ ((__packed__)) uot_keepalive_frame_t;

#pragma pack(pop)

typedef struct _uot_channel_t{

	gnb_connection_t *tcp_conn;

	gnb_connection_t *udp_conn;

	gnb_payload16_ctx_t  *gnb_payload16_ctx;

	gnb_network_service_t *service;

	uint64_t last_reconnect_time_sec;
	uint64_t keepalive_ts_sec;
	uint64_t last_send_keepalive_ts_sec;

}uot_channel_t;

#endif
