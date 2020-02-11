#include "gnb_platform.h"

#include "gnb_event.h"


#if defined(__UNIX_LIKE_OS__)
extern gnb_event_cmd select_event_cmd;
#endif


#if defined(__linux__)
extern gnb_event_cmd epoll_event_cmd;
#endif

#if defined(__FreeBSD__)
extern gnb_event_cmd kqueue_event_cmd;
#endif


#if defined(__OpenBSD__)
extern gnb_event_cmd kqueue_event_cmd;
#endif

#if defined(__APPLE__)
extern gnb_event_cmd kqueue_event_cmd;
#endif


#if defined(_WIN32)
extern gnb_event_cmd select_event_cmd;
#endif


gnb_event_cmd* gnb_create_event_cmd(){

	int i;
	gnb_event_cmd *event_cmd;


#if defined(__FreeBSD__)
	event_cmd = &kqueue_event_cmd;
#endif


#if defined(__OpenBSD__)
	event_cmd = &kqueue_event_cmd;
#endif

#if defined(__APPLE__)
	event_cmd = &kqueue_event_cmd;
#endif


#if defined(__linux__)
	event_cmd = &epoll_event_cmd;
#endif

#if defined(_WIN32)
	event_cmd = &select_event_cmd;
#endif

  	//For Test!!
  	event_cmd = &select_event_cmd;
    //event_cmd = &kqueue_event_cmd;

	return event_cmd;

}

