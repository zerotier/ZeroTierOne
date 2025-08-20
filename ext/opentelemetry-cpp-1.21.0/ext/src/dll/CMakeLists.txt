# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(NOT MSVC)
  message(
    FATAL_ERROR
      "The OpenTelemetry C++ SDK for Windows DLL is only supported on MSVC.")
endif()

set(OPENTELEMETRY_EXPORT_DEF
    "${CMAKE_CURRENT_BINARY_DIR}/opentelemetry_cpp.def")

add_library(opentelemetry_cpp SHARED dllmain.cc ${OPENTELEMETRY_EXPORT_DEF})

set_target_properties(opentelemetry_cpp PROPERTIES EXPORT_NAME
                                                   opentelemetry_cpp)

target_link_libraries(
  opentelemetry_cpp PRIVATE opentelemetry_trace
                            opentelemetry_exporter_ostream_span)

target_include_directories(
  opentelemetry_cpp
  PUBLIC
    $<TARGET_PROPERTY:opentelemetry_trace,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:opentelemetry_exporter_ostream_span,INTERFACE_INCLUDE_DIRECTORIES>
)

if(WITH_OTLP_GRPC)
  target_compile_definitions(opentelemetry_cpp PRIVATE WITH_OTLP_GRPC)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_grpc)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_grpc,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

if(WITH_OTLP_HTTP)
  target_compile_definitions(opentelemetry_cpp PRIVATE WITH_OTLP_HTTP)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_http)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_http,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

if(WITH_OTLP_FILE)
  target_compile_definitions(opentelemetry_cpp PRIVATE WITH_OTLP_FILE)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_file)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_file,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

target_link_libraries(
  opentelemetry_cpp PRIVATE opentelemetry_metrics
                            opentelemetry_exporter_ostream_metrics)
target_include_directories(
  opentelemetry_cpp
  PUBLIC
    $<TARGET_PROPERTY:opentelemetry_metrics,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:opentelemetry_exporter_ostream_metrics,INTERFACE_INCLUDE_DIRECTORIES>
)

if(WITH_OTLP_GRPC)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_grpc_metrics)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_grpc_metrics,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

if(WITH_OTLP_HTTP)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_http_metric)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_http_metric,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

if(WITH_OTLP_FILE)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_file_metric)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_file_metric,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

target_link_libraries(
  opentelemetry_cpp PRIVATE opentelemetry_logs
                            opentelemetry_exporter_ostream_logs)
target_include_directories(
  opentelemetry_cpp
  PUBLIC
    $<TARGET_PROPERTY:opentelemetry_logs,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:opentelemetry_exporter_ostream_logs,INTERFACE_INCLUDE_DIRECTORIES>
)

if(WITH_OTLP_GRPC)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_grpc_log)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_grpc_log,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

if(WITH_OTLP_HTTP)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_http_log)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_http_log,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

if(WITH_OTLP_FILE)
  target_link_libraries(opentelemetry_cpp
                        PRIVATE opentelemetry_exporter_otlp_file_log)
  target_include_directories(
    opentelemetry_cpp
    PUBLIC
      $<TARGET_PROPERTY:opentelemetry_exporter_otlp_file_log,INTERFACE_INCLUDE_DIRECTORIES>
  )
endif()

find_program(
  _vswhere_tool
  NAMES vswhere
  PATHS "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer")
if(NOT ${vswhere})
  message(
    FATAL_ERROR "Could not locate vswhere.exe - unable to locate dumpbin.exe")
endif()

execute_process(
  COMMAND "${_vswhere_tool}" -latest -find **/dumpbin.exe
  OUTPUT_VARIABLE DUMPBIN_PATHS
  RESULT_VARIABLE _vs_where_exitcode
  OUTPUT_STRIP_TRAILING_WHITESPACE)

message(STATUS "DUMPBIN PATHS: ${DUMPBIN_PATHS} ${RESULT_VARIABLE}")

string(REPLACE "\n" ";" DUMPBIN_PATHS_LIST ${DUMPBIN_PATHS})

list(POP_FRONT DUMPBIN_PATHS_LIST DUMPBIN_PATH)

message(STATUS "DUMPBIN PATH: ${DUMPBIN_PATH}")

# find Powershell executable find_program(DUMPBIN_PATH NAMES dumpbin REQUIRED)

# find Powershell executable
find_program(POWERSHELL_PATH NAMES pwsh powershell REQUIRED)

set(OPENTELEMETRY_INPUT_DEF "${CMAKE_CURRENT_SOURCE_DIR}/input.txt")

add_custom_command(
  OUTPUT ${OPENTELEMETRY_INPUT_DEF}
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/input.src"
  COMMAND
    ${CMAKE_CXX_COMPILER}
    "-D$<JOIN:$<TARGET_PROPERTY:opentelemetry_cpp,COMPILE_DEFINITIONS>,;-D>" /EP
    ${CMAKE_CURRENT_SOURCE_DIR}/input.src > ${OPENTELEMETRY_INPUT_DEF}
  VERBATIM COMMAND_EXPAND_LISTS)

add_custom_command(
  OUTPUT ${OPENTELEMETRY_EXPORT_DEF}
  DEPENDS "${OPENTELEMETRY_INPUT_DEF}"
          "${CMAKE_CURRENT_SOURCE_DIR}/make_def.ps1"
  COMMAND
    ${POWERSHELL_PATH} "-ExecutionPolicy" "Bypass" "-File"
    "${CMAKE_CURRENT_SOURCE_DIR}/make_def.ps1" "-dumpbin" "${DUMPBIN_PATH}"
    "-bindir" ${PROJECT_BINARY_DIR} "-inputfile" ${OPENTELEMETRY_INPUT_DEF}
    "-targetfile" ${OPENTELEMETRY_EXPORT_DEF}
  VERBATIM)

otel_add_component(COMPONENT ext_dll TARGETS opentelemetry_cpp)
