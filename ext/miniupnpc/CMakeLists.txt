cmake_minimum_required (VERSION 2.6)

project (miniupnpc C)
set (MINIUPNPC_VERSION 1.9)
set (MINIUPNPC_API_VERSION 15)

if (NOT CMAKE_BUILD_TYPE)
  if (WIN32)
    set (DEFAULT_BUILD_TYPE MinSizeRel)
  else (WIN32)
    set (DEFAULT_BUILD_TYPE RelWithDebInfo)
  endif(WIN32)
    set (CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE} CACHE STRING
        "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()

option (UPNPC_BUILD_STATIC "Build static library" TRUE)
option (UPNPC_BUILD_SHARED "Build shared library" TRUE)
if (NOT WIN32)
  option (UPNPC_BUILD_TESTS "Build test executables" TRUE)
endif (NOT WIN32)
option (NO_GETADDRINFO "Define NO_GETADDRINFO" FALSE)

mark_as_advanced (NO_GETADDRINFO)

if (NO_GETADDRINFO)
  add_definitions (-DNO_GETADDRINFO)
endif (NO_GETADDRINFO)

if (NOT WIN32)
  add_definitions (-DMINIUPNPC_SET_SOCKET_TIMEOUT)
  add_definitions (-D_BSD_SOURCE -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200112L)
else (NOT WIN32)
  add_definitions (-D_WIN32_WINNT=0x0501) # XP or higher for getnameinfo and friends
endif (NOT WIN32)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  add_definitions (-D_DARWIN_C_SOURCE)
endif ()

# Set compiler specific build flags
if (CMAKE_COMPILER_IS_GNUC)
  # Set our own default flags at first run.
  if (NOT CONFIGURED)

    if (NOT CMAKE_SYSTEM_NAME STREQUAL "AmigaOS")
      set (_PIC -fPIC)
    endif (CMAKE_SYSTEM_NAME STREQUAL "AmigaOS")

    set (CMAKE_C_FLAGS "${_PIC} -Wall $ENV{CFLAGS}" # CMAKE_C_FLAGS gets appended to the other C flags
        CACHE STRING "Flags used by the C compiler during normal builds." FORCE)
    set (CMAKE_C_FLAGS_DEBUG "-g -DDDEBUG"
        CACHE STRING "Flags used by the C compiler during debug builds." FORCE)
    set (CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG"
        CACHE STRING "Flags used by the C compiler during release builds." FORCE)
    set (CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG"
        CACHE STRING "Flags used by the C compiler during release builds." FORCE)
    set (CMAKE_C_FLAGS_MINSIZEREL "-Os -DNDEBUG"
        CACHE STRING "Flags used by the C compiler during release builds." FORCE)

  endif (NOT CONFIGURED)
endif ()

configure_file (${CMAKE_SOURCE_DIR}/miniupnpcstrings.h.cmake ${CMAKE_BINARY_DIR}/miniupnpcstrings.h)
include_directories (${CMAKE_BINARY_DIR})

set (MINIUPNPC_SOURCES
  igd_desc_parse.c
  miniupnpc.c
  minixml.c
  minisoap.c
  minissdpc.c
  miniwget.c
  upnpc.c
  upnpcommands.c
  upnpdev.c
  upnpreplyparse.c
  upnperrors.c
  connecthostport.c
  portlistingparse.c
  receivedata.c
)

if (NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "AmigaOS")
  set (MINIUPNPC_SOURCES ${MINIUPNPC_SOURCES} minissdpc.c)
endif (NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "AmigaOS")

if (WIN32)
  set_source_files_properties (${MINIUPNPC_SOURCES} PROPERTIES
                                                    COMPILE_DEFINITIONS "MINIUPNP_STATICLIB;MINIUPNP_EXPORTS"
  )
endif (WIN32)

if (WIN32)
  find_library (WINSOCK2_LIBRARY NAMES ws2_32 WS2_32 Ws2_32)
  find_library (IPHLPAPI_LIBRARY NAMES iphlpapi)
  set (LDLIBS ${WINSOCK2_LIBRARY} ${IPHLPAPI_LIBRARY} ${LDLIBS})
#elseif (CMAKE_SYSTEM_NAME STREQUAL "Solaris")
#  find_library (SOCKET_LIBRARY NAMES socket)
#  find_library (NSL_LIBRARY NAMES nsl)
#  find_library (RESOLV_LIBRARY NAMES resolv)
#  set (LDLIBS ${SOCKET_LIBRARY} ${NSL_LIBRARY} ${RESOLV_LIBRARY} ${LDLIBS})
endif (WIN32)

if (NOT UPNPC_BUILD_STATIC AND NOT UPNPC_BUILD_SHARED)
    message (FATAL "Both shared and static libraries are disabled!")
endif (NOT UPNPC_BUILD_STATIC AND NOT UPNPC_BUILD_SHARED)

if (UPNPC_BUILD_STATIC)
  add_library (upnpc-static STATIC ${MINIUPNPC_SOURCES})
  set_target_properties (upnpc-static PROPERTIES OUTPUT_NAME "miniupnpc")
  target_link_libraries (upnpc-static ${LDLIBS})
  set (UPNPC_INSTALL_TARGETS ${UPNPC_INSTALL_TARGETS} upnpc-static)
  set (UPNPC_LIBRARY_TARGET upnpc-static)
endif (UPNPC_BUILD_STATIC)

if (UPNPC_BUILD_SHARED)
  add_library (upnpc-shared SHARED ${MINIUPNPC_SOURCES})
  set_target_properties (upnpc-shared PROPERTIES OUTPUT_NAME "miniupnpc")
  set_target_properties (upnpc-shared PROPERTIES VERSION ${MINIUPNPC_VERSION})
  set_target_properties (upnpc-shared PROPERTIES SOVERSION ${MINIUPNPC_API_VERSION})
  target_link_libraries (upnpc-shared ${LDLIBS})
  set (UPNPC_INSTALL_TARGETS ${UPNPC_INSTALL_TARGETS} upnpc-shared)
  set (UPNPC_LIBRARY_TARGET upnpc-shared)
endif (UPNPC_BUILD_SHARED)

if (UPNPC_BUILD_TESTS)
  add_executable (testminixml testminixml.c minixml.c igd_desc_parse.c)
  target_link_libraries (testminixml ${LDLIBS})

  add_executable (minixmlvalid minixmlvalid.c minixml.c)
  target_link_libraries (minixmlvalid ${LDLIBS})

  add_executable (testupnpreplyparse testupnpreplyparse.c
                                     minixml.c upnpreplyparse.c)
  target_link_libraries (testupnpreplyparse ${LDLIBS})

  add_executable (testigddescparse testigddescparse.c
                                   igd_desc_parse.c minixml.c miniupnpc.c miniwget.c minissdpc.c
                                   upnpcommands.c upnpreplyparse.c minisoap.c connecthostport.c
                                   portlistingparse.c receivedata.c
  )
  target_link_libraries (testigddescparse ${LDLIBS})

  add_executable (testminiwget testminiwget.c
                               miniwget.c miniupnpc.c minisoap.c upnpcommands.c minissdpc.c
                               upnpreplyparse.c minixml.c igd_desc_parse.c connecthostport.c
                               portlistingparse.c receivedata.c
  )
  target_link_libraries (testminiwget ${LDLIBS})

# set (UPNPC_INSTALL_TARGETS ${UPNPC_INSTALL_TARGETS} testminixml minixmlvalid testupnpreplyparse testigddescparse testminiwget)
endif (UPNPC_BUILD_TESTS)


install (TARGETS ${UPNPC_INSTALL_TARGETS}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib${LIB_SUFFIX}
  ARCHIVE DESTINATION lib${LIB_SUFFIX}
)
install (FILES
	miniupnpc.h
  miniwget.h
  upnpcommands.h
  igd_desc_parse.h
  upnpreplyparse.h
  upnperrors.h
  upnpdev.h
  miniupnpctypes.h
  portlistingparse.h
  miniupnpc_declspec.h
  DESTINATION include/miniupnpc
)

set (CONFIGURED YES CACHE INTERNAL "")

# vim: ts=2:sw=2
