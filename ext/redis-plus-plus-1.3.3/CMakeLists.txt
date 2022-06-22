cmake_minimum_required(VERSION 3.1)

set(REDIS_PLUS_PLUS_VERSION "1.3.3")
message(STATUS "redis-plus-plus version: ${REDIS_PLUS_PLUS_VERSION}")

project(redis++ LANGUAGES CXX VERSION ${REDIS_PLUS_PLUS_VERSION})

set(REDIS_PLUS_PLUS_DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE ${REDIS_PLUS_PLUS_DEFAULT_BUILD_TYPE} CACHE STRING "Set build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()
message(STATUS "redis-plus-plus build type: ${CMAKE_BUILD_TYPE}")

set(REDIS_PLUS_PLUS_DEFAULT_CXX_STANDARD 17)
if(NOT REDIS_PLUS_PLUS_CXX_STANDARD)
    set(REDIS_PLUS_PLUS_CXX_STANDARD ${REDIS_PLUS_PLUS_DEFAULT_CXX_STANDARD} CACHE STRING "Set CXX standard" FORCE)
    set_property(CACHE REDIS_PLUS_PLUS_CXX_STANDARD PROPERTY STRINGS "11" "14" "17" "20")
endif()
message(STATUS "redis-plus-plus build with CXX standard: c++${REDIS_PLUS_PLUS_CXX_STANDARD}")

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${REDIS_PLUS_PLUS_CXX_STANDARD}")
else()
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++${REDIS_PLUS_PLUS_CXX_STANDARD}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${REDIS_PLUS_PLUS_CXX_STANDARD}")
    endif()
endif()

if(REDIS_PLUS_PLUS_BUILD_ASYNC)
    if(REDIS_PLUS_PLUS_BUILD_ASYNC STREQUAL "libuv")
        message(STATUS "redis-plus-plus build async interface with libuv")

        # libuv dependency
        find_path(REDIS_PLUS_PLUS_ASYNC_LIB_HEADER NAMES uv.h)
        find_library(REDIS_PLUS_PLUS_ASYNC_LIB uv)
    else()
        message(FATAL_ERROR "invalid REDIS_PLUS_PLUS_BUILD_ASYNC")
    endif()
endif()

set(REDIS_PLUS_PLUS_SOURCE_DIR src/sw/redis++)

set(REDIS_PLUS_PLUS_SOURCES
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/command.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/command_options.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/connection.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/connection_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/crc16.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/errors.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/pipeline.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/redis.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/redis_cluster.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/reply.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/sentinel.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/shards.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/shards_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/subscriber.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/transaction.cpp"
)

if(REDIS_PLUS_PLUS_BUILD_ASYNC)
    list(APPEND REDIS_PLUS_PLUS_SOURCES
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_connection.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_connection_pool.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_redis.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/event_loop.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_sentinel.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_redis_cluster.cpp"
        "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_shards_pool.cpp"
    )

    if(NOT REDIS_PLUS_PLUS_ASYNC_FUTURE)
        set(REDIS_PLUS_PLUS_ASYNC_FUTURE "std")
    endif()

    if(REDIS_PLUS_PLUS_ASYNC_FUTURE STREQUAL "std")
        set(REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER "${REDIS_PLUS_PLUS_SOURCE_DIR}/future/std")
    elseif(REDIS_PLUS_PLUS_ASYNC_FUTURE STREQUAL "boost")
        set(REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER "${REDIS_PLUS_PLUS_SOURCE_DIR}/future/boost")
        find_package(Boost REQUIRED COMPONENTS system thread)
    else()
        message(FATAL_ERROR "invalid REDIS_PLUS_PLUS_ASYNC_FUTURE")
    endif()
endif()

# cxx utils
if(REDIS_PLUS_PLUS_CXX_STANDARD LESS 17)
    set(CXX_UTILS_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/cxx11")
else()
    set(CXX_UTILS_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/cxx17")
endif()

# TLS support
option(REDIS_PLUS_PLUS_USE_TLS "Build with TLS support" OFF)
message(STATUS "redis-plus-plus TLS support: ${REDIS_PLUS_PLUS_USE_TLS}")

if(REDIS_PLUS_PLUS_USE_TLS)
    set(TLS_SUB_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/tls")

    list(APPEND REDIS_PLUS_PLUS_SOURCES "${TLS_SUB_DIR}/tls.cpp")

    set(REDIS_PLUS_PLUS_DEPENDS "hiredis,hiredis_ssl")
else()
    set(TLS_SUB_DIR "${REDIS_PLUS_PLUS_SOURCE_DIR}/no_tls")

    set(REDIS_PLUS_PLUS_DEPENDS "hiredis")
endif()

# hiredis dependency
find_package(hiredis QUIET)
if(hiredis_FOUND)
    list(APPEND REDIS_PLUS_PLUS_HIREDIS_LIBS hiredis::hiredis)

    if(REDIS_PLUS_PLUS_USE_TLS)
        find_package(hiredis_ssl REQUIRED)
        list(APPEND REDIS_PLUS_PLUS_HIREDIS_LIBS hiredis::hiredis_ssl)
    endif()
else()
    find_path(HIREDIS_HEADER hiredis)
    find_library(HIREDIS_LIB hiredis)
    list(APPEND REDIS_PLUS_PLUS_HIREDIS_LIBS ${HIREDIS_LIB})

    if(REDIS_PLUS_PLUS_USE_TLS)
        find_library(HIREDIS_TLS_LIB hiredis_ssl)
        list(APPEND REDIS_PLUS_PLUS_HIREDIS_LIBS ${HIREDIS_TLS_LIB})
    endif()
endif()

# Build static library
option(REDIS_PLUS_PLUS_BUILD_STATIC "Build static library" ON)
message(STATUS "redis-plus-plus build static library: ${REDIS_PLUS_PLUS_BUILD_STATIC}")

if(REDIS_PLUS_PLUS_BUILD_STATIC)
    set(STATIC_LIB redis++_static)

    add_library(${STATIC_LIB} STATIC ${REDIS_PLUS_PLUS_SOURCES})
    add_library(redis++::${STATIC_LIB} ALIAS ${STATIC_LIB})

    list(APPEND REDIS_PLUS_PLUS_TARGETS ${STATIC_LIB})

    target_include_directories(${STATIC_LIB} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TLS_SUB_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${CXX_UTILS_DIR}>
            $<INSTALL_INTERFACE:include>)

    if(hiredis_FOUND)
        target_link_libraries(${STATIC_LIB} PUBLIC ${REDIS_PLUS_PLUS_HIREDIS_LIBS})
    else()
        target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${HIREDIS_HEADER}>)
    endif()

    if(REDIS_PLUS_PLUS_BUILD_ASYNC)
        target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER}>)
        target_include_directories(${STATIC_LIB} PUBLIC $<BUILD_INTERFACE:${REDIS_PLUS_PLUS_ASYNC_LIB_HEADER}>)
        if(REDIS_PLUS_PLUS_ASYNC_FUTURE STREQUAL "boost")
            target_include_directories(${STATIC_LIB} SYSTEM PUBLIC $<BUILD_INTERFACE:${Boost_INCLUDE_DIR}>)
        endif()
    endif()

    if (WIN32)
        target_compile_definitions(${STATIC_LIB} PRIVATE NOMINMAX)
        set_target_properties(${STATIC_LIB} PROPERTIES CXX_STANDARD ${REDIS_PLUS_PLUS_CXX_STANDARD})
        set_target_properties(${STATIC_LIB} PROPERTIES OUTPUT_NAME redis++_static)
    else()
        target_compile_options(${STATIC_LIB} PRIVATE "-Wall" "-W" "-Werror")
        set_target_properties(${STATIC_LIB} PROPERTIES OUTPUT_NAME redis++)
    endif()

    set_target_properties(${STATIC_LIB} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(${STATIC_LIB} PROPERTIES CXX_EXTENSIONS OFF)

    option(REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC "Build static library with position independent code" ON)
    message(STATUS "redis-plus-plus build static library with position independent code: ${REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC}")

    if(REDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC)
        set_target_properties(${STATIC_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

# Build shared library
option(REDIS_PLUS_PLUS_BUILD_SHARED "Build shared library" ON)
message(STATUS "redis-plus-plus build shared library: ${REDIS_PLUS_PLUS_BUILD_SHARED}")

if(REDIS_PLUS_PLUS_BUILD_SHARED)
    set(SHARED_LIB redis++)

    add_library(${SHARED_LIB} SHARED ${REDIS_PLUS_PLUS_SOURCES})
    add_library(redis++::${SHARED_LIB} ALIAS ${SHARED_LIB})
    
    list(APPEND REDIS_PLUS_PLUS_TARGETS ${SHARED_LIB})

    target_include_directories(${SHARED_LIB} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_SOURCE_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${TLS_SUB_DIR}>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${CXX_UTILS_DIR}>
            $<INSTALL_INTERFACE:include>)

    if(hiredis_FOUND)
        target_link_libraries(${SHARED_LIB} PUBLIC ${REDIS_PLUS_PLUS_HIREDIS_LIBS})
    else()
        target_include_directories(${SHARED_LIB} PUBLIC $<BUILD_INTERFACE:${HIREDIS_HEADER}>)
        target_link_libraries(${SHARED_LIB} PUBLIC ${REDIS_PLUS_PLUS_HIREDIS_LIBS})
    endif()

    if(REDIS_PLUS_PLUS_BUILD_ASYNC)
        target_include_directories(${SHARED_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER}>)
        target_include_directories(${SHARED_LIB} PUBLIC $<BUILD_INTERFACE:${REDIS_PLUS_PLUS_ASYNC_LIB_HEADER}>)
        target_link_libraries(${SHARED_LIB} PUBLIC ${REDIS_PLUS_PLUS_ASYNC_LIB})
        if(REDIS_PLUS_PLUS_ASYNC_FUTURE STREQUAL "boost")
            target_include_directories(${SHARED_LIB} SYSTEM PUBLIC $<BUILD_INTERFACE:${Boost_INCLUDE_DIR}>)
        endif()
    endif()

    if(WIN32)
        target_compile_definitions(${SHARED_LIB} PRIVATE NOMINMAX)
        set_target_properties(${SHARED_LIB} PROPERTIES CXX_STANDARD ${REDIS_PLUS_PLUS_CXX_STANDARD})
        set_target_properties(${SHARED_LIB} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    else()
        target_compile_options(${SHARED_LIB} PRIVATE "-Wall" "-W" "-Werror")
    endif()

    set_target_properties(${SHARED_LIB} PROPERTIES OUTPUT_NAME redis++)
    set_target_properties(${SHARED_LIB} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(${SHARED_LIB} PROPERTIES CXX_EXTENSIONS OFF)
    set_target_properties(${SHARED_LIB} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(${SHARED_LIB} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
endif()

option(REDIS_PLUS_PLUS_BUILD_TEST "Build tests for redis++" ON)
message(STATUS "redis-plus-plus build test: ${REDIS_PLUS_PLUS_BUILD_TEST}")

if(REDIS_PLUS_PLUS_BUILD_TEST)
    add_subdirectory(test)
endif()

install(TARGETS ${REDIS_PLUS_PLUS_TARGETS}
        EXPORT redis++-targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

set(REDIS_PLUS_PLUS_CMAKE_DESTINATION share/cmake/redis++)

install(EXPORT redis++-targets
        FILE redis++-targets.cmake
        NAMESPACE redis++::
        DESTINATION ${REDIS_PLUS_PLUS_CMAKE_DESTINATION})

# Install headers.
set(HEADER_PATH "sw/redis++")
file(GLOB HEADERS "${REDIS_PLUS_PLUS_SOURCE_DIR}/*.h*" "${TLS_SUB_DIR}/*.h" "${CXX_UTILS_DIR}/*.h" "${REDIS_PLUS_PLUS_ASYNC_FUTURE_HEADER}/*.h")
if(NOT REDIS_PLUS_PLUS_BUILD_ASYNC)
    file(GLOB ASYNC_HEADERS "${REDIS_PLUS_PLUS_SOURCE_DIR}/async_*.h" "${REDIS_PLUS_PLUS_SOURCE_DIR}/event_*.h")
    list(REMOVE_ITEM HEADERS ${ASYNC_HEADERS})
endif()
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${HEADER_PATH})

include(CMakePackageConfigHelpers)

write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion)

configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/redis++-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-config.cmake"
        INSTALL_DESTINATION ${REDIS_PLUS_PLUS_CMAKE_DESTINATION})

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-config-version.cmake"
        DESTINATION ${REDIS_PLUS_PLUS_CMAKE_DESTINATION})

export(EXPORT redis++-targets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++-targets.cmake"
        NAMESPACE redis++::)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/redis++.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++.pc" @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/redis++.pc"
        DESTINATION "lib/pkgconfig")

# All the Debian-specific cpack defines.
if(${CMAKE_VERSION} VERSION_GREATER 3.6)
  SET(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS "ON")
endif()
if(NOT DEFINED CPACK_DEBIAN_PACKAGE_DEPENDS)
  SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6, libhiredis-dev")
endif()
SET(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
SET(CPACK_DEBIAN_PACKAGE_VERSION "${REDIS_PLUS_PLUS_VERSION}")
SET(CPACK_DEBIAN_PACKAGE_SOURCE "https://github.com/sewenew/redis-plus-plus")
message(STATUS "Debian package name: ${CPACK_PACKAGE_FILE_NAME}.deb")

# All the common cpack defines.
if(NOT DEFINED CPACK_PACKAGE_NAME)
    SET(CPACK_PACKAGE_NAME "libredis++-dev")
endif()
SET(CPACK_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
SET(CPACK_PACKAGE_DESCRIPTION "A pure C++ client for Redis, based on hiredis.")
SET(CPACK_PACKAGE_CONTACT "anonymous")
SET(CPACK_GENERATOR "DEB")
INCLUDE(CPack)
