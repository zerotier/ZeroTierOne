# Copyright 2021... by Maxim Gusev
#
# https://github.com/John-Jasper-Doe/http-client-lite
#
# Distributed under the MIT License.
# (See accompanying file LICENSE or copy at https://mit-license.org/)

cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(http-client-lite)

## Set output binary
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

## Set property
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Werror -Wextra -Wpedantic -g -O0)


option(HTTP_CLIENT_LITE_OPT_BUILD_EXAMPLES "Build examples" OFF )

if(HTTP_CLIENT_LITE_OPT_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# Interface library:
add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/jdl/httpclientlite.h)
add_custom_target(${PROJECT_NAME}.hdr SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/include/jdl/httpclientlite.h)
target_include_directories(
    ${PROJECT_NAME}
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
