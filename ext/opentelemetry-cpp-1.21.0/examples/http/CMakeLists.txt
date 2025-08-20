# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(http_client client.cc)
add_executable(http_server server.cc)

target_link_libraries(
  http_client
  PRIVATE opentelemetry-cpp::trace opentelemetry-cpp::http_client_curl
          opentelemetry-cpp::ostream_span_exporter ${CURL_LIBRARIES})

target_link_libraries(
  http_server
  PRIVATE opentelemetry-cpp::trace opentelemetry-cpp::http_client_curl
          opentelemetry-cpp::ostream_span_exporter)
