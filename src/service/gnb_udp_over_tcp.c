#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "gnb_network_service.h"
#include "gnb_udp_over_tcp.h"

#include "gnb_log.h"

#define LOG_UDP6                 0x29
#define LOG_UDP4                 0x2A
#define LOG_UDP_TYPE             0x2B

#define SET_LOG_FILE_PATH            0x29
#define SET_LOG_UDP6                 0x2A
#define SET_LOG_UDP4                 0x2B
#define SET_LOG_UDP_TYPE             0x2C
#define SET_CONSOLE_LOG_LEVEL        0x2D


#define SET_FILE_LOG_LEVEL           0x2E
#define SET_UDP_LOG_LEVEL            0x2F


extern gnb_network_service_t uot_udp_service_mod;
extern gnb_network_service_t uot_tcp_service_mod;


static void show_useage(int argc,char *argv[]){

	printf("GNB udp over tcp Service version 1.1.0 protocol version 1.0.0\n");
    printf("Build[%s %s]\n",__DATE__,__TIME__);
	printf("Copyright (C) 2020 gnbdev\n");
	printf("Usage: %s [-u] [-t]\n", argv[0]);
	printf("Command Summary:\n");

	printf("  -t, --tcp                 tcp side\n");
	printf("  -u, --udp                 udp side\n");

	printf("  -l, --listen              listen port\n");

	printf("      --log-udp4            send log to the address ipv4 default is '127.0.0.1:9801'\n");
	printf("      --log-udp-type        the log udp type 'binary' or 'text' default is 'binary'\n");

	printf("      --log-console-level   log-console-level 0-3\n");
	printf("      --log-file-level      log-file-level    0-3\n" );
	printf("      --log-udp-level       log-udp-level     0-3\n");

	printf("      --help\n");

	printf("example:\n");
	printf("%s -u -l listen_udp_port tcp_address tcp_port\n",    argv[0]);
	printf("%s -t -l listen_tcp_port des_address des_udp_port\n",argv[0]);

}



static void setup_log_ctx(gnb_log_ctx_t *log, char *log_path, char *log_udp_sockaddress4_string, uint8_t log_console_level, uint8_t log_file_level, uint8_t log_udp_level){

	int rc;

	log->output_type = GNB_LOG_OUTPUT_STDOUT;


	if ( NULL != log_path ){

		snprintf(log->log_file_path, PATH_MAX, "%s", log_path);

		snprintf(log->log_file_name_std,   PATH_MAX+NAME_MAX, "%s/std.log",   log_path);
		snprintf(log->log_file_name_debug, PATH_MAX+NAME_MAX, "%s/debug.log", log_path);
		snprintf(log->log_file_name_error, PATH_MAX+NAME_MAX, "%s/error.log", log_path);

		log->output_type |= GNB_LOG_OUTPUT_FILE;

	}else{

		log->log_file_path[0] = '\0';

	}

	snprintf(log->config_table[GNB_LOG_ID_EVENT_CORE].log_name, 20,  "GNB_EVENT");
	snprintf(log->config_table[GNB_LOG_ID_UOT].log_name, 20,         "UOT");

    log->config_table[GNB_LOG_ID_EVENT_CORE].console_level               = log_console_level;
    log->config_table[GNB_LOG_ID_EVENT_CORE].file_level                  = log_file_level;
    log->config_table[GNB_LOG_ID_EVENT_CORE].udp_level                   = log_udp_level;

    log->config_table[GNB_LOG_ID_UOT].console_level                      = log_console_level;
    log->config_table[GNB_LOG_ID_UOT].file_level                         = log_file_level;
    log->config_table[GNB_LOG_ID_UOT].udp_level                          = log_udp_level;


    gnb_log_file_rotate(log);

    gnb_log_udp_open(log);

    log->log_payload_type = GNB_EVENT_PAYLOAD_TYPE_UDPLOG;

    if( '\0' != log_udp_sockaddress4_string[0] ){
    	rc = gnb_log_udp_set_addr4_string(log, log_udp_sockaddress4_string);
    	log->output_type |= GNB_LOG_OUTPUT_UDP;
    }

	return;

}



int main (int argc,char *argv[]){

	int udp_opt  = 0;
	int tcp_opt  = 0;

	char     *address = NULL;
	uint16_t   port   = 0;

	uint8_t log_console_level = GNB_LOG_LEVEL1;
	uint8_t log_file_level    = GNB_LOG_LEVEL1;
	uint8_t log_udp_level     = GNB_LOG_LEVEL1;

	uint8_t log_udp_type;

	char *log_path = NULL;

	udp_over_tcp_service_conf_t *udp_over_tcp_service_conf;

	udp_over_tcp_service_conf = (udp_over_tcp_service_conf_t *)malloc(sizeof(udp_over_tcp_service_conf_t));

	memset(udp_over_tcp_service_conf,0,sizeof(udp_over_tcp_service_conf_t));

	char log_udp_sockaddress4_string[16 + 1 + sizeof("65535")];
	memset(log_udp_sockaddress4_string, 0, 16 + 1 + sizeof("65535"));

	int flag;

    struct option long_options[] = {

	  { "udp",                  no_argument, 0, 'u' },
	  { "tcp",                  no_argument, 0, 't' },

	  { "listen",               required_argument, 0, 'l' },

	  { "log-file-path",        required_argument,  0, SET_LOG_FILE_PATH },

	  { "log-udp6",             optional_argument,  &flag, LOG_UDP6 },
	  { "log-udp4",             optional_argument,  &flag, LOG_UDP4 },
	  { "log-udp-type",         required_argument,  0,     LOG_UDP_TYPE },

	  { "log-console-level",    required_argument,  0,   SET_CONSOLE_LOG_LEVEL },
	  { "log-file-level",       required_argument,  0,   SET_FILE_LOG_LEVEL },
	  { "log-udp-level",        required_argument,  0,   SET_UDP_LOG_LEVEL },

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

        case SET_LOG_FILE_PATH:
        	log_path = optarg;
			break;

        case SET_LOG_UDP_TYPE:

        	if ( !strncmp(optarg, "binary", 16) ){
        		log_udp_type = GNB_LOG_UDP_TYPE_BINARY;
        	} else {
        		log_udp_type = GNB_LOG_UDP_TYPE_TEXT;
        	}

        	break;

        case SET_CONSOLE_LOG_LEVEL:
        	log_console_level = (uint8_t)strtoul(optarg, NULL, 10);
        	break;

        case SET_FILE_LOG_LEVEL:
        	log_file_level    = (uint8_t)strtoul(optarg, NULL, 10);
        	break;

        case SET_UDP_LOG_LEVEL:
        	log_udp_level     = (uint8_t)strtoul(optarg, NULL, 10);
        	break;

        case 'h':
        	show_useage(argc,argv);
        	exit(0);

        default:

            break;
        }


        if ( 0 == opt ) {

        	switch (flag) {

        	case LOG_UDP4:

        		if( NULL != optarg ){
        			snprintf(log_udp_sockaddress4_string, 16 + 1 + sizeof("65535"), "%s", optarg);
        		}else{
        			snprintf(log_udp_sockaddress4_string, 16 + 1 + sizeof("65535"), "%s", "127.0.0.1:9801");
        		}

            	break;

        	default:
        		break;
        	}

        	continue;

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


	gnb_log_ctx_t *log;

	log = gnb_log_ctx_create();

	setup_log_ctx(log, log_path, log_udp_sockaddress4_string, log_console_level, log_file_level, log_udp_level);

    gnb_network_service_t *service;

    if ( 0!=udp_opt ){

    	udp_over_tcp_service_conf->tcp_address = address;
		udp_over_tcp_service_conf->tcp_port    = port;

    	service = gnb_network_service_create(&uot_udp_service_mod, log, 1024);
    	gnb_network_service_init(service, udp_over_tcp_service_conf);

    }


    if ( 0!=tcp_opt ){

    	udp_over_tcp_service_conf->des_udp_address = address;
    	udp_over_tcp_service_conf->des_udp_port    = port;

    	service = gnb_network_service_create(&uot_tcp_service_mod, log, 1024);
    	gnb_network_service_init(service, udp_over_tcp_service_conf);

    }


    GNB_STD1(service->log, GNB_LOG_ID_UOT, "log init.\n");


    gnb_network_service_listen(service);

    gnb_network_service_loop(service);

	return 0;


}

