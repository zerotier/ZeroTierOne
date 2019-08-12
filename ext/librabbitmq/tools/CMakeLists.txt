include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${LIBRABBITMQ_INCLUDE_DIRS} ${POPT_INCLUDE_DIR})

if (WIN32)
    set(PLATFORM_DIR win32)
    set(PLATFORM_SRCS
        win32/compat.c
        )
else (WIN32)
    set(PLATFORM_DIR unix)
endif (WIN32)

include_directories(${PLATFORM_DIR})

set(COMMON_SRCS
    common.h
    common.c
    ${PLATFORM_SRCS}
    )

add_executable(amqp-publish publish.c ${COMMON_SRCS})
target_link_libraries(amqp-publish ${RMQ_LIBRARY_TARGET} ${POPT_LIBRARY})

add_executable(amqp-get get.c ${COMMON_SRCS})
target_link_libraries(amqp-get ${RMQ_LIBRARY_TARGET} ${POPT_LIBRARY})

add_executable(amqp-consume consume.c ${PLATFORM_DIR}/process.c ${COMMON_SRCS})
target_link_libraries(amqp-consume ${RMQ_LIBRARY_TARGET} ${POPT_LIBRARY})

add_executable(amqp-declare-queue declare_queue.c ${COMMON_SRCS})
target_link_libraries(amqp-declare-queue ${RMQ_LIBRARY_TARGET} ${POPT_LIBRARY})

add_executable(amqp-delete-queue delete_queue.c ${COMMON_SRCS})
target_link_libraries(amqp-delete-queue ${RMQ_LIBRARY_TARGET} ${POPT_LIBRARY})

if (BUILD_TOOLS_DOCS)
  if (XMLTO_FOUND)
    set(DOCS_SRCS
      doc/amqp-consume.xml
      doc/amqp-declare-queue.xml
      doc/amqp-delete-queue.xml
      doc/amqp-get.xml
      doc/amqp-publish.xml
      doc/librabbitmq-tools.xml
      )

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc)
    set(XMLTO_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/man-date.ent)
    add_custom_command(
            OUTPUT ${XMLTO_DEPENDS}
	          COMMAND date +'%Y-%m-%d' > ${XMLTO_DEPENDS}
            VERBATIM
            )

    set(XMLTO_COMMAND_ARGS --skip-validation --searchpath "${CMAKE_CURRENT_BINARY_DIR}/doc")

    XMLTO(${DOCS_SRCS}
      MODES man
      ALL)

    foreach(file ${XMLTO_FILES_man})
      get_filename_component(fileExt ${file} EXT)
      string( REGEX REPLACE "^[.]" "" fileExt ${fileExt} )
      install(
        FILES ${file}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man${fileExt}
        )
    endforeach()

  else(XMLTO_FOUND)
    message(WARNING "xmlto not found, will not build tools documentation")
  endif(XMLTO_FOUND)
endif()

if (ENABLE_SSL_SUPPORT)
  add_definitions(-DWITH_SSL=1)
endif()

install(TARGETS amqp-publish amqp-get amqp-consume amqp-declare-queue amqp-delete-queue
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PUBLIC_HEADER DESTINATION include)
