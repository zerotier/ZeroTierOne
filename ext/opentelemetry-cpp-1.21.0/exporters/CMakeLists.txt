# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(WITH_OTLP_GRPC
   OR WITH_OTLP_HTTP
   OR WITH_OTLP_FILE)
  add_subdirectory(otlp)
endif()

add_subdirectory(ostream)
add_subdirectory(memory)

if(WITH_PROMETHEUS)
  add_subdirectory(prometheus)
endif()

if(WITH_ZIPKIN)
  add_subdirectory(zipkin)
endif()

if(WITH_ELASTICSEARCH)
  add_subdirectory(elasticsearch)
endif()

if(WITH_ETW)
  add_subdirectory(etw)
endif()
