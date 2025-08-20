# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(WITH_OTLP_GRPC)
  add_executable(func_otlp_grpc func_grpc_main.cc)
  target_link_libraries(func_otlp_grpc ${CMAKE_THREAD_LIBS_INIT}
                        opentelemetry_trace opentelemetry_exporter_otlp_grpc)
endif()

if(WITH_OTLP_HTTP)
  add_executable(func_otlp_http func_http_main.cc)
  target_link_libraries(func_otlp_http ${CMAKE_THREAD_LIBS_INIT}
                        opentelemetry_trace opentelemetry_exporter_otlp_http)
endif()
