# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

#
# The dependency on opentelemetry-proto can be provided different ways. By order
# of decreasing priority, options are:
#
# 1 - Use a provided package
#
# This is useful to build opentelemetry-cpp as part of a super project.
#
# The super project provides the path to the opentelemetry-proto source code
# using variable ${OTELCPP_PROTO_PATH}
#
# 2 - Search for a opentelemetry-proto git submodule
#
# When git submodule is used, the opentelemetry-proto code is located in:
# third_party/opentelemetry-proto
#
# 3 - Download opentelemetry-proto from github
#
# Code from the required version is used, unless a specific release tag is
# provided in variable ${opentelemetry-proto}
#

if(OTELCPP_PROTO_PATH)
  if(NOT EXISTS
     "${OTELCPP_PROTO_PATH}/opentelemetry/proto/common/v1/common.proto")
    message(
      FATAL_ERROR
        "OTELCPP_PROTO_PATH does not point to a opentelemetry-proto repository")
  endif()
  message(STATUS "opentelemetry-proto dependency satisfied by: external path")
  set(PROTO_PATH ${OTELCPP_PROTO_PATH})
  set(needs_proto_download FALSE)
else()
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opentelemetry-proto/.git)
    message(STATUS "opentelemetry-proto dependency satisfied by: git submodule")
    set(PROTO_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opentelemetry-proto")
    set(needs_proto_download FALSE)
  else()
    message(
      STATUS "opentelemetry-proto dependency satisfied by: github download")
    if("${opentelemetry-proto}" STREQUAL "")
      file(READ "${CMAKE_CURRENT_LIST_DIR}/../third_party_release"
           OTELCPP_THIRD_PARTY_RELEASE_CONTENT)
      if(OTELCPP_THIRD_PARTY_RELEASE_CONTENT MATCHES
         "opentelemetry-proto=[ \\t]*([A-Za-z0-9_\\.\\-]+)")
        set(opentelemetry-proto "${CMAKE_MATCH_1}")
      else()
        set(opentelemetry-proto "v1.6.0")
      endif()
      unset(OTELCPP_THIRD_PARTY_RELEASE_CONTENT)
    endif()
    include(ExternalProject)
    ExternalProject_Add(
      opentelemetry-proto
      GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-proto.git
      GIT_TAG "${opentelemetry-proto}"
      UPDATE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
      CONFIGURE_COMMAND ""
      TEST_AFTER_INSTALL 0
      DOWNLOAD_NO_PROGRESS 1
      LOG_CONFIGURE 1
      LOG_BUILD 1
      LOG_INSTALL 1)
    ExternalProject_Get_Property(opentelemetry-proto INSTALL_DIR)
    set(PROTO_PATH "${INSTALL_DIR}/src/opentelemetry-proto")
    set(needs_proto_download TRUE)
  endif()
endif()

set(COMMON_PROTO "${PROTO_PATH}/opentelemetry/proto/common/v1/common.proto")
set(RESOURCE_PROTO
    "${PROTO_PATH}/opentelemetry/proto/resource/v1/resource.proto")
set(TRACE_PROTO "${PROTO_PATH}/opentelemetry/proto/trace/v1/trace.proto")
set(LOGS_PROTO "${PROTO_PATH}/opentelemetry/proto/logs/v1/logs.proto")
set(METRICS_PROTO "${PROTO_PATH}/opentelemetry/proto/metrics/v1/metrics.proto")

set(PROFILES_PROTO
    "${PROTO_PATH}/opentelemetry/proto/profiles/v1development/profiles.proto")

set(TRACE_SERVICE_PROTO
    "${PROTO_PATH}/opentelemetry/proto/collector/trace/v1/trace_service.proto")
set(LOGS_SERVICE_PROTO
    "${PROTO_PATH}/opentelemetry/proto/collector/logs/v1/logs_service.proto")
set(METRICS_SERVICE_PROTO
    "${PROTO_PATH}/opentelemetry/proto/collector/metrics/v1/metrics_service.proto"
)

set(PROFILES_SERVICE_PROTO
    "${PROTO_PATH}/opentelemetry/proto/collector/profiles/v1development/profiles_service.proto"
)

set(GENERATED_PROTOBUF_PATH
    "${CMAKE_BINARY_DIR}/generated/third_party/opentelemetry-proto")

file(MAKE_DIRECTORY "${GENERATED_PROTOBUF_PATH}")

set(COMMON_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/common/v1/common.pb.cc")
set(COMMON_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/common/v1/common.pb.h")
set(RESOURCE_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/resource/v1/resource.pb.cc")
set(RESOURCE_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/resource/v1/resource.pb.h")
set(TRACE_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/trace/v1/trace.pb.cc")
set(TRACE_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/trace/v1/trace.pb.h")
set(LOGS_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/logs/v1/logs.pb.cc")
set(LOGS_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/logs/v1/logs.pb.h")
set(METRICS_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/metrics/v1/metrics.pb.cc")
set(METRICS_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/metrics/v1/metrics.pb.h")

set(TRACE_SERVICE_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/trace/v1/trace_service.pb.cc"
)
set(TRACE_SERVICE_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
)

#
# Notes about the PROFILES signal: - *.proto files added in opentelemetry-proto
# 1.3.0 - C++ code is generated from proto files - The generated code is not
# used yet.
#

set(PROFILES_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/profiles/v1development/profiles.pb.cc"
)
set(PROFILES_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/profiles/v1development/profiles.pb.h"
)
set(PROFILES_SERVICE_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/profiles/v1development/profiles_service.pb.h"
)
set(PROFILES_SERVICE_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/profiles/v1development/profiles_service.pb.cc"
)

if(WITH_OTLP_GRPC)
  set(PROFILES_SERVICE_GRPC_PB_H_FILE
      "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/profiles/v1development/profiles_service.grpc.pb.h"
  )
  set(PROFILES_SERVICE_GRPC_PB_CPP_FILE
      "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/profiles/v1development/profiles_service.grpc.pb.cc"
  )
endif()

if(WITH_OTLP_GRPC)
  set(TRACE_SERVICE_GRPC_PB_CPP_FILE
      "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.cc"
  )
  set(TRACE_SERVICE_GRPC_PB_H_FILE
      "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/trace/v1/trace_service.grpc.pb.h"
  )
endif()
set(LOGS_SERVICE_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/logs/v1/logs_service.pb.cc"
)
set(LOGS_SERVICE_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/logs/v1/logs_service.pb.h"
)
if(WITH_OTLP_GRPC)
  set(LOGS_SERVICE_GRPC_PB_CPP_FILE
      "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.cc"
  )
  set(LOGS_SERVICE_GRPC_PB_H_FILE
      "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/logs/v1/logs_service.grpc.pb.h"
  )
endif()
set(METRICS_SERVICE_PB_CPP_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.cc"
)
set(METRICS_SERVICE_PB_H_FILE
    "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/metrics/v1/metrics_service.pb.h"
)
if(WITH_OTLP_GRPC)
  set(METRICS_SERVICE_GRPC_PB_CPP_FILE
      "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.cc"
  )
  set(METRICS_SERVICE_GRPC_PB_H_FILE
      "${GENERATED_PROTOBUF_PATH}/opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"
  )
endif()

foreach(IMPORT_DIR ${PROTOBUF_IMPORT_DIRS})
  list(APPEND PROTOBUF_INCLUDE_FLAGS "-I${IMPORT_DIR}")
endforeach()

if(WITH_OTLP_GRPC)
  if(CMAKE_CROSSCOMPILING)
    find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
  else()
    if(TARGET gRPC::grpc_cpp_plugin)
      project_build_tools_get_imported_location(gRPC_CPP_PLUGIN_EXECUTABLE
                                                gRPC::grpc_cpp_plugin)
    else()
      find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    endif()
  endif()
  message(STATUS "gRPC_CPP_PLUGIN_EXECUTABLE=${gRPC_CPP_PLUGIN_EXECUTABLE}")
endif()

set(PROTOBUF_COMMON_FLAGS "--proto_path=${PROTO_PATH}"
                          "--cpp_out=${GENERATED_PROTOBUF_PATH}")
# --experimental_allow_proto3_optional is available from 3.13 and be stable and
# enabled by default from 3.16
if(Protobuf_VERSION AND Protobuf_VERSION VERSION_LESS "3.16")
  list(APPEND PROTOBUF_COMMON_FLAGS "--experimental_allow_proto3_optional")
elseif(PROTOBUF_VERSION AND PROTOBUF_VERSION VERSION_LESS "3.16")
  list(APPEND PROTOBUF_COMMON_FLAGS "--experimental_allow_proto3_optional")
endif()

set(PROTOBUF_GENERATED_FILES
    ${COMMON_PB_H_FILE}
    ${COMMON_PB_CPP_FILE}
    ${RESOURCE_PB_H_FILE}
    ${RESOURCE_PB_CPP_FILE}
    ${TRACE_PB_H_FILE}
    ${TRACE_PB_CPP_FILE}
    ${LOGS_PB_H_FILE}
    ${LOGS_PB_CPP_FILE}
    ${METRICS_PB_H_FILE}
    ${METRICS_PB_CPP_FILE}
    ${PROFILES_H_FILE}
    ${PROFILES_CPP_FILE}
    ${TRACE_SERVICE_PB_H_FILE}
    ${TRACE_SERVICE_PB_CPP_FILE}
    ${LOGS_SERVICE_PB_H_FILE}
    ${LOGS_SERVICE_PB_CPP_FILE}
    ${METRICS_SERVICE_PB_H_FILE}
    ${METRICS_SERVICE_PB_CPP_FILE}
    ${PROFILES_SERVICE_PB_H_FILE}
    ${PROFILES_SERVICE_PB_CPP_FILE})

if(WITH_OTLP_GRPC)
  list(APPEND PROTOBUF_COMMON_FLAGS
       "--grpc_out=generate_mock_code=true:${GENERATED_PROTOBUF_PATH}"
       --plugin=protoc-gen-grpc="${gRPC_CPP_PLUGIN_EXECUTABLE}")

  list(
    APPEND
    PROTOBUF_GENERATED_FILES
    ${TRACE_SERVICE_GRPC_PB_H_FILE}
    ${TRACE_SERVICE_GRPC_PB_CPP_FILE}
    ${LOGS_SERVICE_GRPC_PB_H_FILE}
    ${LOGS_SERVICE_GRPC_PB_CPP_FILE}
    ${METRICS_SERVICE_GRPC_PB_H_FILE}
    ${METRICS_SERVICE_GRPC_PB_CPP_FILE}
    ${PROFILES_SERVICE_GRPC_PB_H_FILE}
    ${PROFILES_SERVICE_GRPC_PB_CPP_FILE})
endif()

set(PROTOBUF_RUN_PROTOC_COMMAND "\"${PROTOBUF_PROTOC_EXECUTABLE}\"")
foreach(
  PROTOBUF_RUN_PROTOC_ARG
  ${PROTOBUF_COMMON_FLAGS}
  ${PROTOBUF_INCLUDE_FLAGS}
  ${COMMON_PROTO}
  ${RESOURCE_PROTO}
  ${TRACE_PROTO}
  ${LOGS_PROTO}
  ${METRICS_PROTO}
  ${PROFILES_PROTO}
  ${TRACE_SERVICE_PROTO}
  ${LOGS_SERVICE_PROTO}
  ${METRICS_SERVICE_PROTO}
  ${PROFILES_SERVICE_PROTO})
  set(PROTOBUF_RUN_PROTOC_COMMAND
      "${PROTOBUF_RUN_PROTOC_COMMAND} \"${PROTOBUF_RUN_PROTOC_ARG}\"")
endforeach()

add_custom_command(
  OUTPUT ${PROTOBUF_GENERATED_FILES}
  COMMAND
    ${PROTOBUF_PROTOC_EXECUTABLE} ${PROTOBUF_COMMON_FLAGS}
    ${PROTOBUF_INCLUDE_FLAGS} ${COMMON_PROTO} ${RESOURCE_PROTO} ${TRACE_PROTO}
    ${LOGS_PROTO} ${METRICS_PROTO} ${TRACE_SERVICE_PROTO} ${LOGS_SERVICE_PROTO}
    ${METRICS_SERVICE_PROTO} ${PROFILES_PROTO} ${PROFILES_SERVICE_PROTO}
  COMMENT "[Run]: ${PROTOBUF_RUN_PROTOC_COMMAND}"
  DEPENDS ${PROTOBUF_PROTOC_EXECUTABLE})

unset(OTELCPP_PROTO_TARGET_OPTIONS)
if(CMAKE_SYSTEM_NAME MATCHES "Windows|MinGW|WindowsStore")
  list(APPEND OTELCPP_PROTO_TARGET_OPTIONS STATIC)
elseif(NOT DEFINED BUILD_SHARED_LIBS OR BUILD_SHARED_LIBS)
  list(APPEND OTELCPP_PROTO_TARGET_OPTIONS SHARED)
else()
  list(APPEND OTELCPP_PROTO_TARGET_OPTIONS STATIC)
endif()

set(OPENTELEMETRY_PROTO_TARGETS opentelemetry_proto)
add_library(
  opentelemetry_proto
  ${OTELCPP_PROTO_TARGET_OPTIONS}
  ${COMMON_PB_CPP_FILE}
  ${RESOURCE_PB_CPP_FILE}
  ${TRACE_PB_CPP_FILE}
  ${LOGS_PB_CPP_FILE}
  ${METRICS_PB_CPP_FILE}
  ${TRACE_SERVICE_PB_CPP_FILE}
  ${LOGS_SERVICE_PB_CPP_FILE}
  ${METRICS_SERVICE_PB_CPP_FILE})
set_target_version(opentelemetry_proto)

target_include_directories(
  opentelemetry_proto
  PUBLIC "$<BUILD_INTERFACE:${GENERATED_PROTOBUF_PATH}>"
         "$<INSTALL_INTERFACE:include>")

# Disable include-what-you-use on generated code.
set_target_properties(opentelemetry_proto PROPERTIES CXX_INCLUDE_WHAT_YOU_USE
                                                     "")
if(NOT Protobuf_INCLUDE_DIRS AND TARGET protobuf::libprotobuf)
  get_target_property(Protobuf_INCLUDE_DIRS protobuf::libprotobuf
                      INTERFACE_INCLUDE_DIRECTORIES)
endif()
if(Protobuf_INCLUDE_DIRS)
  target_include_directories(
    opentelemetry_proto BEFORE
    PUBLIC "$<BUILD_INTERFACE:${Protobuf_INCLUDE_DIRS}>")
endif()

if(WITH_OTLP_GRPC)
  add_library(
    opentelemetry_proto_grpc
    ${OTELCPP_PROTO_TARGET_OPTIONS} ${TRACE_SERVICE_GRPC_PB_CPP_FILE}
    ${LOGS_SERVICE_GRPC_PB_CPP_FILE} ${METRICS_SERVICE_GRPC_PB_CPP_FILE})
  set_target_version(opentelemetry_proto_grpc)

  # Disable include-what-you-use on generated code.
  set_target_properties(opentelemetry_proto_grpc
                        PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "")

  list(APPEND OPENTELEMETRY_PROTO_TARGETS opentelemetry_proto_grpc)
  target_link_libraries(opentelemetry_proto_grpc PUBLIC opentelemetry_proto)

  get_target_property(grpc_lib_type gRPC::grpc++ TYPE)
  if(grpc_lib_type STREQUAL "SHARED_LIBRARY")
    target_link_libraries(opentelemetry_proto_grpc PUBLIC gRPC::grpc++)
  endif()
  set_target_properties(opentelemetry_proto_grpc PROPERTIES EXPORT_NAME
                                                            proto_grpc)
  patch_protobuf_targets(opentelemetry_proto_grpc)
  get_target_property(GRPC_INCLUDE_DIRECTORY gRPC::grpc++
                      INTERFACE_INCLUDE_DIRECTORIES)
  if(GRPC_INCLUDE_DIRECTORY)
    target_include_directories(
      opentelemetry_proto_grpc BEFORE
      PUBLIC "$<BUILD_INTERFACE:${GRPC_INCLUDE_DIRECTORY}>")
  endif()
endif()

if(needs_proto_download)
  add_dependencies(opentelemetry_proto opentelemetry-proto)
endif()
set_target_properties(opentelemetry_proto PROPERTIES EXPORT_NAME proto)
patch_protobuf_targets(opentelemetry_proto)

if(OPENTELEMETRY_INSTALL)
  install(
    DIRECTORY ${GENERATED_PROTOBUF_PATH}/opentelemetry
    DESTINATION include
    COMPONENT exporters_otlp_common
    FILES_MATCHING
    PATTERN "*.h")
endif()

if(TARGET protobuf::libprotobuf)
  target_link_libraries(opentelemetry_proto PUBLIC protobuf::libprotobuf)
else() # cmake 3.8 or lower
  target_link_libraries(opentelemetry_proto PUBLIC ${Protobuf_LIBRARIES})
endif()

# this is needed on some older grcp versions specifically conan recipe for grpc/1.54.3
if(WITH_OTLP_GRPC)
  if(TARGET absl::synchronization)
    target_link_libraries(opentelemetry_proto_grpc
                          PUBLIC "$<BUILD_INTERFACE:absl::synchronization>")
  endif()
endif()

if(BUILD_SHARED_LIBS)
  foreach(proto_target ${OPENTELEMETRY_PROTO_TARGETS})
    set_property(TARGET ${proto_target} PROPERTY POSITION_INDEPENDENT_CODE ON)
  endforeach()
endif()
