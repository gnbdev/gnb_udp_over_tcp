#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "gnb_network_service.h"
#include "gnb_udp_over_tcp.h"

extern gnb_network_service_t uot_udp_service_mod;
extern gnb_network_service_t uot_tcp_service_mod;


static void show_useage(int argc,char *argv[]){

	printf("GNB udp over tcp Service version 1.0.0 protocol version 1.0.0\n");
    printf("Build[%s %s]\n",__DATE__,__TIME__);
	printf("Copyright (C) 2020 gnbdev\n");
	printf("Usage: %s [-u] [-t]\n", argv[0]);
	printf("Command Summary:\n");

	printf("  -t, --tcp                 tcp side\n");
	printf("  -u, --udp                 udp side\n");

	printf("  -l, --listen              listen port\n");

	printf("      --help\n");

	printf("example:\n");
	printf("%s -u -l listen_udp_port tcp_address tcp_port\n",    argv[0]);
	printf("%s -t -l listen_tcp_port des_address des_udp_port\n",argv[0]);

}



int main (int argc,char *argv[]){

	int udp_opt  = 0;
	int tcp_opt  = 0;

	char     *address = NULL;
	uint16_t   port   = 0;

	udp_over_tcp_service_conf_t *udp_over_tcp_service_conf;

	udp_over_tcp_service_conf = (udp_over_tcp_service_conf_t *)malloc(sizeof(udp_over_tcp_service_conf_t));

	memset(udp_over_tcp_service_conf,0,sizeof(udp_over_tcp_service_conf_t));

    static struct option long_options[] = {

	  { "udp",                  no_argument, 0, 'u' },
	  { "tcp",                  no_argument, 0, 't' },

	  { "listen",               required_argument, 0, 'l' },

	  { "help",                 no_argument, 0, 'h' },

	  { 0, 0, 0, 0 }

    };


    int opt;

    while (1) {

        int option_index = 0;

        opt = getopt_long (argc, argv, "l:uth",long_options, &option_index);


        if (opt == -1) {
        	break;
        }

        switch (opt) {

        case 'l':
        	udp_over_tcp_service_conf->listen_port = (uint16_t)strtol(optarg, NULL, 0);
            break;

        case 'u':
        	udp_opt = 1;

            break;

        case 't':
        	tcp_opt = 1;

            break;

        case 'h':
        	show_useage(argc,argv);
        	exit(0);

        default:

            break;
        }

    }


    if( 0 == udp_over_tcp_service_conf->listen_port ){
    	show_useage(argc,argv);
    	exit(0);
    }


    if ( (optind+2) == argc ) {

    	address = argv[optind];
    	port    = (uint16_t)strtol((const char *)argv[optind+1], NULL, 0);

    }else{

    	show_useage(argc,argv);
    	exit(0);
    }


    if ( (0==udp_opt && 0==tcp_opt) || (0!=udp_opt && 0!=tcp_opt) ){
    	show_useage(argc,argv);
    	exit(0);
    }

    gnb_network_service_t *service;

    if ( 0!=udp_opt ){

    	udp_over_tcp_service_conf->tcp_address = address;
		udp_over_tcp_service_conf->tcp_port    = port;

    	service = gnb_network_service_create(&uot_udp_service_mod, 1024);
    	gnb_network_service_init(service, udp_over_tcp_service_conf);

    }


    if ( 0!=tcp_opt){

    	udp_over_tcp_service_conf->des_udp_address = address;
    	udp_over_tcp_service_conf->des_udp_port    = port;

    	service = gnb_network_service_create(&uot_tcp_service_mod, 1024);
    	gnb_network_service_init(service, udp_over_tcp_service_conf);

    }

    gnb_network_service_listen(service);

    gnb_network_service_loop(service);

	return 0;


}

