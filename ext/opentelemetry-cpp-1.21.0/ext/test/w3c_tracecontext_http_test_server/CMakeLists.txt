# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(w3c_tracecontext_http_test_server main.cc)
target_link_libraries(
  w3c_tracecontext_http_test_server
  PRIVATE ${CMAKE_THREAD_LIBS_INIT} opentelemetry_trace
          opentelemetry_http_client_curl opentelemetry_exporter_ostream_span
          ${CURL_LIBRARIES} nlohmann_json::nlohmann_json)
if(nlohmann_json_clone)
  add_dependencies(w3c_tracecontext_http_test_server
                   nlohmann_json::nlohmann_json)
endif()
