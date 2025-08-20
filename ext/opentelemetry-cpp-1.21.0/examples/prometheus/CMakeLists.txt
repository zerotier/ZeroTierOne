# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(prometheus_example main.cc)
target_link_libraries(
  prometheus_example PRIVATE opentelemetry-cpp::prometheus_exporter
                             common_metrics_foo_library)
