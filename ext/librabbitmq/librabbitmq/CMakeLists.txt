project(librabbitmq "C")

if (REGENERATE_AMQP_FRAMING)
  set(AMQP_CODEGEN_PY "${CMAKE_CURRENT_BINARY_DIR}/amqp_codegen.py")
  set(CODEGEN_PY "${CMAKE_CURRENT_BINARY_DIR}/codegen.py")
  set(AMQP_SPEC_JSON_PATH "${AMQP_CODEGEN_DIR}/amqp-rabbitmq-0.9.1.json")
  set(AMQP_FRAMING_H_PATH ${CMAKE_CURRENT_BINARY_DIR}/amqp_framing.h)
  set(AMQP_FRAMING_C_PATH ${CMAKE_CURRENT_BINARY_DIR}/amqp_framing.c)

  if (PYTHON_VERSION_MAJOR GREATER 2)
    set(CONVERT_CODEGEN ${PYTHON_2TO3_EXECUTABLE} -w ${CODEGEN_PY} > codegen_2to3.out)
    set(CONVERT_AMQP_CODEGEN ${PYTHON_2TO3_EXECUTABLE} -w ${AMQP_CODEGEN_PY} > amqp_codegen_2to3.out)
  else ()
    set(CONVERT_CODEGEN "")
    set(CONVERT_AMQP_CODEGEN "")
  endif ()

  add_custom_command(
    OUTPUT ${CODEGEN_PY}
    COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/codegen.py ${CODEGEN_PY}
    COMMAND ${CONVERT_CODEGEN}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/codegen.py
    VERBATIM)

  add_custom_command(
    OUTPUT ${AMQP_CODEGEN_PY}
    COMMAND ${CMAKE_COMMAND} ARGS -E copy ${AMQP_CODEGEN_DIR}/amqp_codegen.py ${AMQP_CODEGEN_PY}
    COMMAND ${CONVERT_AMQP_CODEGEN}
    DEPENDS ${AMQP_CODEGEN_DIR}/amqp_codegen.py ${AMQP_CODEGEN_TARGET}
    VERBATIM)

  add_custom_command(
    OUTPUT ${AMQP_FRAMING_H_PATH}
    COMMAND ${PYTHON_EXECUTABLE} ARGS ${CODEGEN_PY} header ${AMQP_SPEC_JSON_PATH} ${AMQP_FRAMING_H_PATH}
    DEPENDS ${AMQP_SPEC_JSON_PATH} ${CODEGEN_PY} ${AMQP_CODEGEN_PY}
    VERBATIM)

  add_custom_command(
    OUTPUT ${AMQP_FRAMING_C_PATH}
    COMMAND ${PYTHON_EXECUTABLE} ARGS ${CODEGEN_PY} body ${AMQP_SPEC_JSON_PATH} ${AMQP_FRAMING_C_PATH}
    DEPENDS ${AMQP_SPEC_JSON_PATH} ${CODEGEN_PY} ${AMQP_CODEGEN_PY}
    VERBATIM)
else (REGENERATE_AMQP_FRAMING)
  set(AMQP_FRAMING_H_PATH ${CMAKE_CURRENT_SOURCE_DIR}/amqp_framing.h)
  set(AMQP_FRAMING_C_PATH ${CMAKE_CURRENT_SOURCE_DIR}/amqp_framing.c)
endif (REGENERATE_AMQP_FRAMING)

if(WIN32)
  set(SOCKET_IMPL "win32")
else(WIN32)
  set(SOCKET_IMPL "unix")
endif(WIN32)

if(MSVC)
  if(MSVC_VERSION LESS 1600)
    set(MSINTTYPES_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/win32/msinttypes")
    set(STDINT_H_INSTALL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/win32/msinttypes/stdint.h")
  endif(MSVC_VERSION LESS 1600)
endif(MSVC)

# NOTE: order is important here: if we generate amqp_framing.h/.c it'll be in the
# binary directory, and should shadow whats in the source directory
set(LIBRABBITMQ_INCLUDE_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${SOCKET_IMPL}
	${MSINTTYPES_INCLUDE}
	)

include_directories(${LIBRABBITMQ_INCLUDE_DIRS})

set(LIBRABBITMQ_INCLUDE_DIRS
	${LIBRABBITMQ_INCLUDE_DIRS}
	PARENT_SCOPE)

add_definitions(-DHAVE_CONFIG_H)

if (ENABLE_SSL_SUPPORT)
  add_definitions(-DWITH_SSL=1)
  set(AMQP_SSL_SOCKET_H_PATH amqp_ssl_socket.h)

  set(AMQP_SSL_SRCS ${AMQP_SSL_SOCKET_H_PATH}
      amqp_openssl.c
      amqp_openssl_hostname_validation.c
      amqp_openssl_hostname_validation.h
      amqp_hostcheck.c
      amqp_hostcheck.h
      amqp_openssl_bio.c
      amqp_openssl_bio.h
  )
  include_directories(${OPENSSL_INCLUDE_DIR})
  set(AMQP_SSL_LIBS ${OPENSSL_LIBRARIES})
  if (APPLE)
    # Apple has deprecated OpenSSL in 10.7+. This disables that warning.
    set_source_files_properties(${AMQP_SSL_SRCS}
      PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
  endif()

  if (WIN32)
    set(AMQP_SSL_SRCS ${AMQP_SSL_SRCS} win32/threads.h win32/threads.c)
  else()
    set(AMQP_SSL_SRCS ${AMQP_SSL_SRCS} unix/threads.h)
  endif()
endif()

set(RABBITMQ_SOURCES
    ${AMQP_FRAMING_H_PATH}
    ${AMQP_FRAMING_C_PATH}
    amqp_api.c amqp.h amqp_connection.c amqp_mem.c amqp_private.h amqp_socket.c
    amqp_table.c amqp_url.c amqp_socket.h amqp_tcp_socket.c amqp_tcp_socket.h
    amqp_time.c amqp_time.h
    amqp_consumer.c
    ${AMQP_SSL_SRCS}
)

add_definitions(-DAMQP_BUILD)

set(RMQ_LIBRARIES ${AMQP_SSL_LIBS} ${SOCKET_LIBRARIES} ${LIBRT} ${CMAKE_THREAD_LIBS_INIT})

if (BUILD_SHARED_LIBS)
    add_library(rabbitmq SHARED ${RABBITMQ_SOURCES})
    if (THREADS_HAVE_PTHREAD_ARG)
      target_compile_options(rabbitmq PUBLIC "-pthread")
    endif()

    target_link_libraries(rabbitmq ${RMQ_LIBRARIES})

    if (WIN32)
        set_target_properties(rabbitmq PROPERTIES VERSION ${RMQ_VERSION} OUTPUT_NAME rabbitmq.${RMQ_SOVERSION})
    else (WIN32)
        set_target_properties(rabbitmq PROPERTIES VERSION ${RMQ_VERSION} SOVERSION ${RMQ_SOVERSION})
    endif (WIN32)

    install(TARGETS rabbitmq
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )

    set(RMQ_LIBRARY_TARGET rabbitmq)
endif (BUILD_SHARED_LIBS)

if (BUILD_STATIC_LIBS)
    add_library(rabbitmq-static STATIC ${RABBITMQ_SOURCES})
    if (THREADS_HAVE_PTHREAD_ARG)
      target_compile_options(rabbitmq-static PUBLIC "-pthread")
    endif()

    target_link_libraries(rabbitmq-static ${RMQ_LIBRARIES})

    set_target_properties(rabbitmq-static PROPERTIES COMPILE_DEFINITIONS AMQP_STATIC)
    if (WIN32)
        set_target_properties(rabbitmq-static PROPERTIES
          VERSION ${RMQ_VERSION}
          OUTPUT_NAME librabbitmq.${RMQ_SOVERSION})

        if(MSVC)
            set_target_properties(rabbitmq-static PROPERTIES
            # Embed debugging info in the library itself instead of generating
            # a .pdb file.
            COMPILE_OPTIONS "/Z7")
        endif(MSVC)

    else (WIN32)
        set_target_properties(rabbitmq-static PROPERTIES VERSION ${RMQ_VERSION} SOVERSION ${RMQ_SOVERSION} OUTPUT_NAME rabbitmq)
    endif (WIN32)

    install(TARGETS rabbitmq-static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )

    if (NOT DEFINED RMQ_LIBRARY_TARGET)
        set(RMQ_LIBRARY_TARGET rabbitmq-static)
    endif ()
endif (BUILD_STATIC_LIBS)

install(FILES
  amqp.h
  ${AMQP_FRAMING_H_PATH}
  amqp_tcp_socket.h
  ${AMQP_SSL_SOCKET_H_PATH}
  ${STDINT_H_INSTALL_FILE}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

set(RMQ_LIBRARY_TARGET ${RMQ_LIBRARY_TARGET} PARENT_SCOPE)
