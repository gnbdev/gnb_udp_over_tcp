#ifndef gnb_udp_over_tcp_h
#define gnb_udp_over_tcp_h

#include <stdint.h>

typedef struct _udp_over_tcp_service_conf_t{

	int tcp;
	int udp;

	uint16_t  listen_port;

	char     *tcp_address;
	uint16_t  tcp_port;


    char     *des_udp_address;
    uint16_t  des_udp_port;

}udp_over_tcp_service_conf_t;


#endif

