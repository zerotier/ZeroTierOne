# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set(this_target opentelemetry_opentracing_shim)

add_library(${this_target} src/shim_utils.cc src/span_shim.cc
                           src/span_context_shim.cc src/tracer_shim.cc)

set_target_properties(${this_target} PROPERTIES EXPORT_NAME opentracing_shim)
set_target_version(${this_target})

target_include_directories(
  ${this_target} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
                        "$<INSTALL_INTERFACE:include>")

if(OPENTRACING_DIR)
  target_include_directories(
    ${this_target}
    PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/${OPENTRACING_DIR}/include>"
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${OPENTRACING_DIR}/include>"
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/${OPENTRACING_DIR}/3rd_party/include>"
  )
  target_link_libraries(${this_target} opentelemetry_api opentracing)
else()
  target_link_libraries(${this_target} opentelemetry_api
                        OpenTracing::opentracing)
endif()

otel_add_component(
  COMPONENT
  shims_opentracing
  TARGETS
  ${this_target}
  FILES_DIRECTORY
  "include/opentelemetry/opentracingshim"
  FILES_DESTINATION
  "include/opentelemetry/"
  FILES_MATCHING
  PATTERN
  "*.h")

if(BUILD_TESTING)
  foreach(testname propagation_test shim_utils_test span_shim_test
                   span_context_shim_test tracer_shim_test)

    add_executable(${testname} "test/${testname}.cc")

    if(OPENTRACING_DIR)
      target_link_libraries(
        ${testname} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
        opentelemetry_api opentelemetry_opentracing_shim opentracing)
    else()
      target_link_libraries(
        ${testname} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
        opentelemetry_api opentelemetry_opentracing_shim
        OpenTracing::opentracing)
    endif()

    gtest_add_tests(
      TARGET ${testname}
      TEST_PREFIX opentracing_shim.
      TEST_LIST ${testname})
  endforeach()
endif() # BUILD_TESTING
