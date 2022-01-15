GNB_EVENT_OBJS =                           \
       ./src/gnb_event.o                   \
       ./src/gnb_network_service.o         \
       ./src/gnb_address.o                 \
       ./src/gnb_buf.o                     \
       ./src/gnb_lru32.o                   \
       ./src/gnb_doubly_linked_list.o      \
       ./src/gnb_hash32.o                  \
       ./libs/hash/murmurhash.o            \
       ./src/gnb_payload16.o               \
       ./src/gnb_time.o                    \
       ./src/gnb_fixed_list.o              \
       ./src/gnb_fixed_list_store.o        \
       ./src/gnb_fixed_pool.o              \
       ./src/gnb_alloc.o


GNB_UDP_OVER_TCP_OBJS =                    \
       ./src/service/gnb_udp_over_tcp.o    \
       ./src/service/uot_udp_service.o     \
       ./src/service/uot_tcp_service.o     \
       ./src/gnb_daemon.o                  \
       ./src/gnb_log.o

