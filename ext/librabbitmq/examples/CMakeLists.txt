if (NOT BUILD_SHARED_LIBS)
  add_definitions(-DAMQP_STATIC)
endif()

include_directories(${LIBRABBITMQ_INCLUDE_DIRS})

if (WIN32)
    set(PLATFORM_DIR win32)
else (WIN32)
    set(PLATFORM_DIR unix)
endif (WIN32)

set(COMMON_SRCS
    utils.h
    utils.c
    ${PLATFORM_DIR}/platform_utils.c
    )

add_executable(amqp_sendstring amqp_sendstring.c ${COMMON_SRCS})
target_link_libraries(amqp_sendstring ${RMQ_LIBRARY_TARGET})

add_executable(amqp_rpc_sendstring_client amqp_rpc_sendstring_client.c ${COMMON_SRCS})
target_link_libraries(amqp_rpc_sendstring_client ${RMQ_LIBRARY_TARGET})

add_executable(amqp_exchange_declare amqp_exchange_declare.c ${COMMON_SRCS})
target_link_libraries(amqp_exchange_declare ${RMQ_LIBRARY_TARGET})

add_executable(amqp_listen amqp_listen.c ${COMMON_SRCS})
target_link_libraries(amqp_listen ${RMQ_LIBRARY_TARGET})

add_executable(amqp_producer amqp_producer.c ${COMMON_SRCS})
target_link_libraries(amqp_producer ${RMQ_LIBRARY_TARGET})

add_executable(amqp_connect_timeout amqp_connect_timeout.c ${COMMON_SRCS})
target_link_libraries(amqp_connect_timeout ${RMQ_LIBRARY_TARGET})

add_executable(amqp_consumer amqp_consumer.c ${COMMON_SRCS})
target_link_libraries(amqp_consumer ${RMQ_LIBRARY_TARGET})

add_executable(amqp_unbind amqp_unbind.c ${COMMON_SRCS})
target_link_libraries(amqp_unbind ${RMQ_LIBRARY_TARGET})

add_executable(amqp_bind amqp_bind.c ${COMMON_SRCS})
target_link_libraries(amqp_bind ${RMQ_LIBRARY_TARGET})

add_executable(amqp_listenq amqp_listenq.c ${COMMON_SRCS})
target_link_libraries(amqp_listenq ${RMQ_LIBRARY_TARGET})

if (ENABLE_SSL_SUPPORT)
add_executable(amqp_ssl_connect amqp_ssl_connect.c ${COMMON_SRCS})
target_link_libraries(amqp_ssl_connect ${RMQ_LIBRARY_TARGET})
endif (ENABLE_SSL_SUPPORT)
