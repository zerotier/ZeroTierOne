# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(WITH_OTLP_GRPC)
  # TRACE

  add_executable(example_otlp_grpc grpc_main.cc)

  target_link_libraries(example_otlp_grpc PRIVATE common_foo_library)

  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_grpc
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_grpc
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(example_otlp_grpc
                          PRIVATE opentelemetry-cpp::otlp_grpc_exporter)
  endif()

  # METRIC

  add_executable(example_otlp_grpc_metric grpc_metric_main.cc)

  target_link_libraries(example_otlp_grpc_metric
                        PRIVATE common_metrics_foo_library)

  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_grpc_metric
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_grpc_metric
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(example_otlp_grpc_metric
                          PRIVATE opentelemetry-cpp::otlp_grpc_metrics_exporter)
  endif()

  # LOG

  add_executable(example_otlp_grpc_log grpc_log_main.cc)

  target_link_libraries(example_otlp_grpc_log PRIVATE common_logs_foo_library)
  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_grpc_log
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_grpc_log
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(
      example_otlp_grpc_log
      PRIVATE opentelemetry-cpp::otlp_grpc_exporter
              opentelemetry-cpp::otlp_grpc_log_record_exporter)
  endif()
endif()

if(WITH_OTLP_HTTP)
  # TRACE

  add_executable(example_otlp_http http_main.cc)

  target_link_libraries(example_otlp_http PRIVATE common_foo_library)
  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_http
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_http
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(example_otlp_http
                          PRIVATE opentelemetry-cpp::otlp_http_exporter)
  endif()

  # METRIC

  add_executable(example_otlp_http_metric http_metric_main.cc)
  target_link_libraries(example_otlp_http_metric
                        PRIVATE common_metrics_foo_library)

  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_http_metric
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_http_metric
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(example_otlp_http_metric
                          PRIVATE opentelemetry-cpp::otlp_http_metric_exporter)
  endif()

  # LOG

  add_executable(example_otlp_http_log http_log_main.cc)
  target_link_libraries(example_otlp_http_log PRIVATE common_logs_foo_library)

  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_http_log
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_http_log
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(
      example_otlp_http_log
      PRIVATE opentelemetry-cpp::otlp_http_exporter
              opentelemetry-cpp::otlp_http_log_record_exporter)
  endif()

  # ALL, instrumented

  add_executable(example_otlp_instrumented_http http_instrumented_main.cc)

  # Note: common_logs_foo_library provide traces and logs
  target_link_libraries(
    example_otlp_instrumented_http PRIVATE common_metrics_foo_library
                                           common_logs_foo_library)

  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_instrumented_http
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_instrumented_http
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(
      example_otlp_instrumented_http
      PRIVATE opentelemetry-cpp::otlp_http_exporter
              opentelemetry-cpp::otlp_http_metric_exporter
              opentelemetry-cpp::otlp_http_log_record_exporter)
  endif()

endif()

if(WITH_OTLP_FILE)
  # TRACE

  add_executable(example_otlp_file file_main.cc)

  target_link_libraries(example_otlp_file PRIVATE common_foo_library)

  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_file
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_file
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(example_otlp_file
                          PRIVATE opentelemetry-cpp::otlp_file_exporter)
  endif()

  # METRIC

  add_executable(example_otlp_file_metric file_metric_main.cc)

  target_link_libraries(example_otlp_file_metric
                        PRIVATE common_metrics_foo_library)

  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_file_metric
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_file_metric
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(example_otlp_file_metric
                          PRIVATE opentelemetry-cpp::otlp_file_metric_exporter)
  endif()

  # LOG

  add_executable(example_otlp_file_log file_log_main.cc)

  target_link_libraries(example_otlp_file_log PRIVATE common_logs_foo_library)
  if(DEFINED OPENTELEMETRY_BUILD_DLL)
    target_compile_definitions(example_otlp_file_log
                               PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
    target_link_libraries(example_otlp_file_log
                          PRIVATE opentelemetry-cpp::opentelemetry_cpp)
  else()
    target_link_libraries(
      example_otlp_file_log
      PRIVATE opentelemetry-cpp::otlp_file_exporter
              opentelemetry-cpp::otlp_file_log_record_exporter)
  endif()
endif()
