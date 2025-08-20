# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)

# See https://cmake.org/cmake/help/latest/policy/CMP0074.html required by
# certain version of zlib which CURL depends on.
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.12")
  cmake_policy(SET CMP0074 NEW)
endif()

# Allow to use normal variable for option()
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13")
  cmake_policy(SET CMP0077 NEW)
endif()

# Prefer CMAKE_MSVC_RUNTIME_LIBRARY if possible
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

if(POLICY CMP0092)
  # https://cmake.org/cmake/help/latest/policy/CMP0092.html#policy:CMP0092 Make
  # sure the /W3 is not removed from CMAKE_CXX_FLAGS since CMake 3.15
  cmake_policy(SET CMP0092 OLD)
endif()

# MSVC RTTI flag /GR should not be not added to CMAKE_CXX_FLAGS by default. @see
# https://cmake.org/cmake/help/latest/policy/CMP0117.html
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.20.0")
  cmake_policy(SET CMP0117 NEW)
endif()

project(opentelemetry-cpp)

# Mark variables as used so cmake doesn't complain about them
mark_as_advanced(CMAKE_TOOLCHAIN_FILE)

# Note: CMAKE_FIND_PACKAGE_PREFER_CONFIG requires cmake 3.15. Prefer cmake
# CONFIG search mode to find dependencies. This is important to properly find
# protobuf versions 3.22.0 and above due to the abseil-cpp dependency.
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# Don't use customized cmake modules if vcpkg is used to resolve dependence.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party_release")
  file(STRINGS "${CMAKE_SOURCE_DIR}/third_party_release" third_party_tags)
  foreach(third_party ${third_party_tags})
    string(REGEX REPLACE "^[ ]+" "" third_party ${third_party})
    string(REGEX MATCH "^[^=]+" third_party_name ${third_party})
    string(REPLACE "${third_party_name}=" "" third_party_tag ${third_party})
    set(${third_party_name} "${third_party_tag}")
  endforeach()
endif()

if(DEFINED ENV{ARCH})
  # Architecture may be specified via ARCH environment variable
  set(ARCH $ENV{ARCH})
else()
  # Autodetection logic that populates ARCH variable
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|x86_64.*|AMD64.*")
    # Windows may report AMD64 even if target is 32-bit
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(ARCH x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(ARCH x86)
    endif()
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*")
    # Windows may report x86 even if target is 64-bit
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(ARCH x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(ARCH x86)
    endif()
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "powerpc")
    # AIX will report the processor as 'powerpc' even if building in 64-bit mode
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(ARCH ppc64)
    else()
      set(ARCH ppc32)
    endif()
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES
         "^(aarch64.*|AARCH64.*|arm64.*|ARM64.*)")
    set(ARCH arm64)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm.*|ARM.*)")
    set(ARCH arm)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64le")
    set(ARCH ppc64le)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(powerpc|ppc)64")
    set(ARCH ppc64)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(mips.*|MIPS.*)")
    set(ARCH mips)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(riscv.*|RISCV.*)")
    set(ARCH riscv)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(s390x.*|S390X.*)")
    set(ARCH s390x)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(sparc.*|SPARC.*)")
    set(ARCH sparc)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(loongarch.*|LOONGARCH.*)")
    set(ARCH loongarch)
  else()
    message(
      FATAL_ERROR
        "opentelemetry-cpp: unrecognized target processor ${CMAKE_SYSTEM_PROCESSOR} configuration!"
    )
  endif()
endif()
message(STATUS "Building for architecture ARCH=${ARCH}")

# Autodetect vcpkg toolchain
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE
      "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "")
endif()

if(VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
  include("${VCPKG_CHAINLOAD_TOOLCHAIN_FILE}")
endif()

option(WITH_ABI_VERSION_1 "ABI version 1" ON)
option(WITH_ABI_VERSION_2 "EXPERIMENTAL: ABI version 2 preview" OFF)

file(READ "${CMAKE_CURRENT_LIST_DIR}/api/include/opentelemetry/version.h"
     OPENTELEMETRY_CPP_HEADER_VERSION_H)

#
# We do not want to have WITH_ABI_VERSION = "1" or "2", and instead prefer two
# distinct flags, WITH_ABI_VERSION_1 and WITH_ABI_VERSION_2.
#
# This allows:
#
# * to have a specific option description for each ABI
# * to mark experimental/stable/deprecated on flags, for clarity
# * to search for exact abi usage move easily, discouraging:
#
#   * cmake -DWITH_ABI_VERSION=${ARG}
#
# While not supported, having distinct WITH_ABI_VERSION_1 and WITH_ABI_VERSION_2
# flags also opens the possibility to support multiple ABI concurrently, should
# that become necessary.
#
if(WITH_ABI_VERSION_1 AND WITH_ABI_VERSION_2)
  #
  # Only one ABI is supported in a build.
  #
  message(
    FATAL_ERROR "Set either WITH_ABI_VERSION_1 or WITH_ABI_VERSION_2, not both")
endif()

if(WITH_ABI_VERSION_2)
  set(OPENTELEMETRY_ABI_VERSION_NO "2")
elseif(WITH_ABI_VERSION_1)
  set(OPENTELEMETRY_ABI_VERSION_NO "1")
else()
  if(OPENTELEMETRY_CPP_HEADER_VERSION_H MATCHES
     "OPENTELEMETRY_ABI_VERSION_NO[ \t\r\n]+\"?([0-9]+)\"?")
    math(EXPR OPENTELEMETRY_ABI_VERSION_NO ${CMAKE_MATCH_1})
  else()
    message(
      FATAL_ERROR
        "OPENTELEMETRY_ABI_VERSION_NO not found on ${CMAKE_CURRENT_LIST_DIR}/api/include/opentelemetry/version.h"
    )
  endif()
endif()

message(STATUS "OPENTELEMETRY_ABI_VERSION_NO=${OPENTELEMETRY_ABI_VERSION_NO}")

if(OPENTELEMETRY_CPP_HEADER_VERSION_H MATCHES
   "OPENTELEMETRY_VERSION[ \t\r\n]+\"?([^\"]+)\"?")
  set(OPENTELEMETRY_VERSION ${CMAKE_MATCH_1})
else()
  message(
    FATAL_ERROR
      "OPENTELEMETRY_VERSION not found on ${CMAKE_CURRENT_LIST_DIR}/api/include/opentelemetry/version.h"
  )
endif()

message(STATUS "OPENTELEMETRY_VERSION=${OPENTELEMETRY_VERSION}")

option(WITH_NO_DEPRECATED_CODE "Do not include deprecated code" OFF)

set(WITH_STL
    "OFF"
    CACHE STRING "Which version of the Standard Library for C++ to use")

option(WITH_GSL
       "Whether to use Guidelines Support Library for C++ latest features" OFF)

set(OPENTELEMETRY_INSTALL_default ON)
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(OPENTELEMETRY_INSTALL_default OFF)
endif()
option(OPENTELEMETRY_INSTALL "Whether to install opentelemetry targets"
       ${OPENTELEMETRY_INSTALL_default})

include("${PROJECT_SOURCE_DIR}/cmake/tools.cmake")

if(NOT WITH_STL STREQUAL "OFF")
  # These definitions are needed for test projects that do not link against
  # opentelemetry-api library directly. We ensure that variant implementation
  # (absl::variant or std::variant) in variant unit test code is consistent with
  # the global project build definitions. Optimize for speed to reduce the hops
  if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_BUILD_TYPE MATCHES Debug)
      # Turn off optimizations for DEBUG
      set(MSVC_CXX_OPT_FLAG "/Od")
    else()
      string(REGEX MATCH "\/O" result ${CMAKE_CXX_FLAGS})
      if(NOT ${result} MATCHES "\/O")
        set(MSVC_CXX_OPT_FLAG "/O2")
      endif()
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MSVC_CXX_OPT_FLAG}")
  endif()
endif()

option(WITH_OTLP_RETRY_PREVIEW
       "Whether to enable experimental retry functionality" OFF)

option(WITH_OTLP_GRPC_SSL_MTLS_PREVIEW
       "Whether to enable mTLS support fro gRPC" OFF)

option(WITH_OTLP_GRPC "Whether to include the OTLP gRPC exporter in the SDK"
       OFF)

option(WITH_OTLP_HTTP "Whether to include the OTLP http exporter in the SDK"
       OFF)

option(WITH_OTLP_FILE "Whether to include the OTLP file exporter in the SDK"
       OFF)

option(
  WITH_OTLP_HTTP_COMPRESSION
  "Whether to include gzip compression for the OTLP http exporter in the SDK"
  OFF)

option(WITH_CURL_LOGGING "Whether to enable select CURL verbosity in OTel logs"
       OFF)

option(WITH_ZIPKIN "Whether to include the Zipkin exporter in the SDK" OFF)

option(WITH_PROMETHEUS "Whether to include the Prometheus Client in the SDK"
       OFF)

option(WITH_ELASTICSEARCH
       "Whether to include the Elasticsearch Client in the SDK" OFF)

option(WITH_NO_GETENV "Whether the platform supports environment variables" OFF)

option(BUILD_TESTING "Whether to enable tests" ON)

option(WITH_BENCHMARK "Whether to build benchmark program" ON)

option(BUILD_W3CTRACECONTEXT_TEST "Whether to build w3c trace context" OFF)

option(OTELCPP_MAINTAINER_MODE "Build in maintainer mode (-Wall -Werror)" OFF)

option(WITH_OPENTRACING "Whether to include the Opentracing shim" OFF)

option(OTELCPP_VERSIONED_LIBS "Whether to generate the versioned shared libs"
       OFF)

#
# This option is experimental, subject to change in the spec:
#
# * https://github.com/open-telemetry/opentelemetry-specification/issues/2232
#
option(WITH_REMOVE_METER_PREVIEW
       "EXPERIMENTAL, ABI BREAKING: Allow to remove a meter" OFF)

if(OTELCPP_VERSIONED_LIBS AND NOT BUILD_SHARED_LIBS)
  message(FATAL_ERROR "OTELCPP_VERSIONED_LIBS=ON requires BUILD_SHARED_LIBS=ON")
endif()

if(WIN32)
  option(WITH_ETW "Whether to include the ETW Exporter in the SDK" ON)
else()
  if(DEFINED (WITH_ETW))
    message(FATAL_ERROR "WITH_ETW is only supported on Windows")
  endif()
endif(WIN32)

# Do not convert deprecated message to error
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  add_compile_options(-Wno-error=deprecated-declarations)
endif()

option(
  WITH_API_ONLY
  "Only build the API (use as a header-only library). Overrides WITH_EXAMPLES and all options to enable exporters"
  OFF)
option(WITH_EXAMPLES "Whether to build examples" ON)

# This requires CURL, OFF by default.
option(
  WITH_EXAMPLES_HTTP
  "Whether to build http client/server examples. Requires WITH_EXAMPLES and CURL"
  OFF)

option(WITH_FUNC_TESTS "Whether to build functional tests" ON)

option(WITH_ASYNC_EXPORT_PREVIEW "Whether to enable async export" OFF)

# Exemplar specs status is experimental, so behind feature flag by default
option(WITH_METRICS_EXEMPLAR_PREVIEW
       "Whether to enable exemplar within metrics" OFF)

# Experimental, so behind feature flag by default
option(WITH_THREAD_INSTRUMENTATION_PREVIEW
       "Whether to enable thread instrumentation" OFF)

option(OPENTELEMETRY_SKIP_DYNAMIC_LOADING_TESTS
       "Whether to build test libraries that are always linked as shared libs"
       OFF)

#
# Verify options dependencies
#

if(WITH_EXAMPLES_HTTP AND NOT WITH_EXAMPLES)
  message(FATAL_ERROR "WITH_EXAMPLES_HTTP=ON requires WITH_EXAMPLES=ON")
endif()

find_package(Threads)

function(install_windows_deps)
  # Bootstrap vcpkg from CMake and auto-install deps in case if we are missing
  # deps on Windows. Respect the target architecture variable.
  set(VCPKG_TARGET_ARCHITECTURE
      ${ARCH}
      PARENT_SCOPE)
  message(STATUS "Installing build tools and dependencies...")
  set(ENV{ARCH} ${ARCH})
  execute_process(
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/setup-buildtools.cmd)
  set(CMAKE_TOOLCHAIN_FILE
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/vcpkg/scripts/buildsystems/vcpkg.cmake
      CACHE FILEPATH "")
  message(
    STATUS
      "Make sure that vcpkg.cmake is set as the CMAKE_TOOLCHAIN_FILE at the START of the cmake build process!
    Can be command-line arg (cmake -DCMAKE_TOOLCHAIN_FILE=...) or set in your editor of choice."
  )

endfunction()

function(set_target_version target_name)
  if(OTELCPP_VERSIONED_LIBS)
    set_target_properties(
      ${target_name} PROPERTIES VERSION ${OPENTELEMETRY_VERSION}
                                SOVERSION ${OPENTELEMETRY_ABI_VERSION_NO})
  endif()
endfunction()

if(MSVC)
  # Options for Visual C++ compiler: /Zc:__cplusplus - report an updated value
  # for recent C++ language standards. Without this option MSVC returns the
  # value of __cplusplus="199711L"
  if(MSVC_VERSION GREATER 1900)
    # __cplusplus flag is not supported by Visual Studio 2015
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
  endif()
endif()

# include GNUInstallDirs before include cmake/opentelemetry-proto.cmake because
# opentelemetry-proto installs targets to the location variables defined in
# GNUInstallDirs.
include(GNUInstallDirs)

if(WITH_PROMETHEUS)
  find_package(prometheus-cpp CONFIG QUIET)
  if(NOT prometheus-cpp_FOUND)
    message(STATUS "Trying to use local prometheus-cpp from submodule")
    if(EXISTS ${PROJECT_SOURCE_DIR}/third_party/prometheus-cpp/.git)
      set(SAVED_ENABLE_TESTING ${ENABLE_TESTING})
      set(SAVED_CMAKE_CXX_CLANG_TIDY ${CMAKE_CXX_CLANG_TIDY})
      set(SAVED_CMAKE_CXX_INCLUDE_WHAT_YOU_USE
          ${CMAKE_CXX_INCLUDE_WHAT_YOU_USE})
      set(ENABLE_TESTING OFF)
      set(CMAKE_CXX_CLANG_TIDY "")
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "")
      add_subdirectory(third_party/prometheus-cpp)
      set(ENABLE_TESTING ${SAVED_ENABLE_TESTING})
      set(CMAKE_CXX_CLANG_TIDY ${SAVED_CMAKE_CXX_CLANG_TIDY})
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
          ${SAVED_CMAKE_CXX_INCLUDE_WHAT_YOU_USE})

      # Get the version of the prometheus-cpp submodule
      find_package(Git QUIET)
      if(Git_FOUND)
        execute_process(
          COMMAND ${GIT_EXECUTABLE} describe --tags --always
          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/third_party/prometheus-cpp
          OUTPUT_VARIABLE prometheus-cpp_VERSION
          OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX REPLACE "^v" "" prometheus-cpp_VERSION
                             "${prometheus-cpp_VERSION}")
      endif()

      message(
        STATUS
          "Using local prometheus-cpp from submodule. Version = ${prometheus-cpp_VERSION}"
      )
    else()
      message(
        FATAL_ERROR
          "\nprometheus-cpp package was not found. Please either provide it manually or clone with submodules. "
          "To initialize, fetch and checkout any nested submodules, you can use the following command:\n"
          "git submodule update --init --recursive")
    endif()
  else()
    message(STATUS "Using external prometheus-cpp")
  endif()
endif()

if(WITH_OTLP_GRPC
   OR WITH_OTLP_HTTP
   OR WITH_OTLP_FILE)

  # Including the CMakeFindDependencyMacro resolves an error from
  # gRPCConfig.cmake on some grpc versions. See
  # https://github.com/grpc/grpc/pull/33361 for more details.
  include(CMakeFindDependencyMacro)

  # Protobuf 3.22+ depends on abseil-cpp and must be found using the cmake
  # find_package CONFIG search mode. The following attempts to find Protobuf
  # using the CONFIG mode first, and if not found, falls back to the MODULE
  # mode. See https://gitlab.kitware.com/cmake/cmake/-/issues/24321 for more
  # details.
  find_package(Protobuf CONFIG)
  if(NOT Protobuf_FOUND)
    find_package(Protobuf MODULE)
    if(Protobuf_FOUND AND Protobuf_VERSION VERSION_GREATER_EQUAL "3.22.0")
      message(
        WARNING
          "Found Protobuf version ${Protobuf_VERSION} using MODULE mode. "
          "Linking errors may occur. Protobuf 3.22+ depends on abseil-cpp "
          "and should be found using the CONFIG mode.")
    endif()
  endif()

  if(WITH_OTLP_GRPC)
    find_package(gRPC CONFIG)
  endif()
  if((NOT Protobuf_FOUND) OR (WITH_OTLP_GRPC AND NOT gRPC_FOUND))
    if(WIN32 AND (NOT DEFINED CMAKE_TOOLCHAIN_FILE))
      install_windows_deps()
    endif()

    if(WIN32 AND (NOT DEFINED CMAKE_TOOLCHAIN_FILE))
      message(FATAL_ERROR "Windows dependency installation failed!")
    endif()
    if(WIN32)
      include(${CMAKE_TOOLCHAIN_FILE})
    endif()

    if(NOT Protobuf_FOUND)
      find_package(Protobuf CONFIG REQUIRED)
    endif()
    if(NOT gRPC_FOUND AND WITH_OTLP_GRPC)
      find_package(gRPC CONFIG)
    endif()
    if(WIN32)
      # Always use x64 protoc.exe
      if(NOT EXISTS "${Protobuf_PROTOC_EXECUTABLE}")
        set(Protobuf_PROTOC_EXECUTABLE
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/vcpkg/packages/protobuf_x64-windows/tools/protobuf/protoc.exe
        )
      endif()
    endif()
  endif()
  # Latest Protobuf imported targets and without legacy module support
  if(TARGET protobuf::protoc)
    if(CMAKE_CROSSCOMPILING AND Protobuf_PROTOC_EXECUTABLE)
      set(PROTOBUF_PROTOC_EXECUTABLE ${Protobuf_PROTOC_EXECUTABLE})
    else()
      project_build_tools_get_imported_location(PROTOBUF_PROTOC_EXECUTABLE
                                                protobuf::protoc)
      # If protobuf::protoc is not a imported target, then we use the target
      # directly for fallback
      if(NOT PROTOBUF_PROTOC_EXECUTABLE)
        set(PROTOBUF_PROTOC_EXECUTABLE protobuf::protoc)
      endif()
    endif()
  elseif(Protobuf_PROTOC_EXECUTABLE)
    # Some versions of FindProtobuf.cmake uses mixed case instead of uppercase
    set(PROTOBUF_PROTOC_EXECUTABLE ${Protobuf_PROTOC_EXECUTABLE})
  endif()
  include(CMakeDependentOption)

  message(STATUS "PROTOBUF_PROTOC_EXECUTABLE=${PROTOBUF_PROTOC_EXECUTABLE}")
  set(SAVED_CMAKE_CXX_CLANG_TIDY ${CMAKE_CXX_CLANG_TIDY})
  set(CMAKE_CXX_CLANG_TIDY "")
  include(cmake/opentelemetry-proto.cmake)
  set(CMAKE_CXX_CLANG_TIDY ${SAVED_CMAKE_CXX_CLANG_TIDY})
endif()

#
# Do we need HTTP CLIENT CURL ?
#

if(WITH_OTLP_HTTP
   OR WITH_ELASTICSEARCH
   OR WITH_ZIPKIN
   OR BUILD_W3CTRACECONTEXT_TEST
   OR WITH_EXAMPLES_HTTP)
  set(WITH_HTTP_CLIENT_CURL ON)
else()
  set(WITH_HTTP_CLIENT_CURL OFF)
endif()

#
# Do we need CURL ?
#

if((NOT WITH_API_ONLY) AND WITH_HTTP_CLIENT_CURL)
  # No specific version required.
  find_package(CURL REQUIRED)
  # Set the CURL_VERSION from the legacy CURL_VERSION_STRING Required for CMake
  # versions below 4.0
  if(NOT DEFINED CURL_VERSION AND DEFINED CURL_VERSION_STRING)
    set(CURL_VERSION ${CURL_VERSION_STRING})
  endif()
endif()

#
# Do we need ZLIB ?
#

if((NOT WITH_API_ONLY)
   AND WITH_HTTP_CLIENT_CURL
   AND WITH_OTLP_HTTP_COMPRESSION)
  # No specific version required.
  find_package(ZLIB REQUIRED)
  # Set the ZLIB_VERSION from the legacy ZLIB_VERSION_STRING Required for CMake
  # versions below 3.26
  if(NOT DEFINED ZLIB_VERSION AND DEFINED ZLIB_VERSION_STRING)
    set(ZLIB_VERSION ${ZLIB_VERSION_STRING})
  endif()
endif()

#
# Do we need NLOHMANN_JSON ?
#

if(WITH_ELASTICSEARCH
   OR WITH_ZIPKIN
   OR WITH_OTLP_HTTP
   OR WITH_OTLP_FILE
   OR BUILD_W3CTRACECONTEXT_TEST
   OR WITH_ETW)
  set(USE_NLOHMANN_JSON ON)
else()
  set(USE_NLOHMANN_JSON OFF)
endif()

if((NOT WITH_API_ONLY) AND USE_NLOHMANN_JSON)
  include(cmake/nlohmann-json.cmake)
endif()

if(OTELCPP_MAINTAINER_MODE)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "Building with gcc in maintainer mode.")

    add_compile_options(-Wall)
    add_compile_options(-Werror)
    add_compile_options(-Wextra)

    # Tested with GCC 9.4 on github.
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 9.4)
      message(STATUS "Building with additional warnings for gcc.")

      # Relaxed warnings

      # Enforced warnings

      # C++ options only
      add_compile_options($<$<STREQUAL:$<COMPILE_LANGUAGE>,CXX>:-Wextra-semi>)
      add_compile_options(
        $<$<STREQUAL:$<COMPILE_LANGUAGE>,CXX>:-Woverloaded-virtual>)
      add_compile_options(
        $<$<STREQUAL:$<COMPILE_LANGUAGE>,CXX>:-Wsuggest-override>)
      add_compile_options(
        $<$<STREQUAL:$<COMPILE_LANGUAGE>,CXX>:-Wold-style-cast>)

      # C and C++
      add_compile_options(-Wcast-qual)
      add_compile_options(-Wformat-security)
      add_compile_options(-Wlogical-op)
      add_compile_options(-Wmissing-include-dirs)
      add_compile_options(-Wstringop-truncation)
      add_compile_options(-Wundef)
      add_compile_options(-Wvla)
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Building with clang in maintainer mode.")

    add_compile_options(-Wall)
    add_compile_options(-Werror)
    add_compile_options(-Wextra)

    # Tested with Clang 11.0 on github.
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11.0)
      message(STATUS "Building with additional warnings for clang.")

      # Relaxed warnings
      add_compile_options(-Wno-error=unused-private-field)

      # Enforced warnings
      add_compile_options(-Wcast-qual)
      add_compile_options(-Wconditional-uninitialized)
      add_compile_options(-Wextra-semi)
      add_compile_options(-Wformat-security)
      add_compile_options(-Wheader-hygiene)
      add_compile_options(-Winconsistent-missing-destructor-override)
      add_compile_options(-Winconsistent-missing-override)
      add_compile_options(-Wnewline-eof)
      add_compile_options(-Wnon-virtual-dtor)
      add_compile_options(-Woverloaded-virtual)
      add_compile_options(-Wrange-loop-analysis)
      add_compile_options(-Wundef)
      add_compile_options(-Wundefined-reinterpret-cast)
      add_compile_options(-Wvla)
      add_compile_options(-Wold-style-cast)
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(STATUS "Building with msvc in maintainer mode.")

    add_compile_options(/WX)
    add_compile_options(/W4)

    # Relaxed warnings
    add_compile_options(/wd4100)
    add_compile_options(/wd4125)
    add_compile_options(/wd4566)
    add_compile_options(/wd4127)
    add_compile_options(/wd4512)
    add_compile_options(/wd4267)
    add_compile_options(/wd4996)

    # Enforced warnings
    add_compile_options(/we4265) # 'class': class has virtual functions, but
                                 # destructor is not virtual
    add_compile_options(/we5204) # A class with virtual functions has
                                 # non-virtual trivial destructor.

  elseif()
    message(FATAL_ERROR "Building with unknown compiler in maintainer mode.")
  endif()
endif(OTELCPP_MAINTAINER_MODE)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")

include(CTest)
if(BUILD_TESTING)
  if(EXISTS ${CMAKE_BINARY_DIR}/lib/libgtest.a)
    # Prefer GTest from build tree. GTest is not always working with
    # CMAKE_PREFIX_PATH
    set(GTEST_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/googletest/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/googlemock/include)
    if(TARGET gtest)
      set(GTEST_BOTH_LIBRARIES gtest gtest_main)
    else()
      set(GTEST_BOTH_LIBRARIES ${CMAKE_BINARY_DIR}/lib/libgtest.a
                               ${CMAKE_BINARY_DIR}/lib/libgtest_main.a)
    endif()
  elseif(WIN32)
    # Make sure we are always bootsrapped with vcpkg on Windows
    find_package(GTest)
    if(NOT (GTEST_FOUND OR GTest_FOUND))
      if(DEFINED CMAKE_TOOLCHAIN_FILE)
        message(
          FATAL_ERROR
            "Pleaes install GTest with the CMAKE_TOOLCHAIN_FILE at ${CMAKE_TOOLCHAIN_FILE}"
        )
      else()
        install_windows_deps()
        include(${CMAKE_TOOLCHAIN_FILE})
        find_package(GTest REQUIRED)
      endif()
    endif()
  else()
    # Prefer GTest installed by OS distro, brew or vcpkg package manager
    find_package(GTest REQUIRED)
  endif()
  if(NOT GTEST_BOTH_LIBRARIES)
    # New GTest package names
    if(TARGET GTest::gtest)
      set(GTEST_BOTH_LIBRARIES GTest::gtest GTest::gtest_main)
    elseif(TARGET GTest::GTest)
      set(GTEST_BOTH_LIBRARIES GTest::GTest GTest::Main)
    endif()
  endif()
  if(GTEST_INCLUDE_DIRS)
    include_directories(SYSTEM ${GTEST_INCLUDE_DIRS})
  endif()
  message(STATUS "GTEST_INCLUDE_DIRS   = ${GTEST_INCLUDE_DIRS}")
  message(STATUS "GTEST_BOTH_LIBRARIES = ${GTEST_BOTH_LIBRARIES}")

  # Try to find gmock
  if(NOT GMOCK_LIB AND TARGET GTest::gmock)
    set(GMOCK_LIB GTest::gmock)
  elseif(MSVC)
    # Explicitly specify that we consume GTest from shared library. The rest of
    # code logic below determines whether we link Release or Debug flavor of the
    # library. These flavors have different prefix on Windows, gmock and gmockd
    # respectively.
    add_definitions(-DGTEST_LINKED_AS_SHARED_LIBRARY=1)
    if(GMOCK_LIB)
      # unset GMOCK_LIB to force find_library to redo the lookup, as the cached
      # entry could cause linking to incorrect flavor of gmock and leading to
      # runtime error.
      unset(GMOCK_LIB CACHE)
    endif()
  endif()
  if(NOT GMOCK_LIB)
    if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
      find_library(GMOCK_LIB gmockd PATH_SUFFIXES lib)
    else()
      find_library(GMOCK_LIB gmock PATH_SUFFIXES lib)
    endif()
    # Reset GMOCK_LIB if it was not found, or some target may link
    # GMOCK_LIB-NOTFOUND
    if(NOT GMOCK_LIB)
      unset(GMOCK_LIB)
      unset(GMOCK_LIB CACHE)
    endif()
  endif()

  enable_testing()
  if(WITH_BENCHMARK)
    # Benchmark respects the CMAKE_PREFIX_PATH
    find_package(benchmark CONFIG REQUIRED)
  endif()
endif()

# Record build config and versions
message(STATUS "---------------------------------------------")
message(STATUS "build settings")
message(STATUS "---------------------------------------------")
message(STATUS "OpenTelemetry: ${OPENTELEMETRY_VERSION}")
message(STATUS "OpenTelemetry ABI: ${OPENTELEMETRY_ABI_VERSION_NO}")
message(STATUS "ARCH: ${ARCH}")
message(STATUS "CXX: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXXFLAGS: ${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_STANDARD: ${CMAKE_CXX_STANDARD}")
message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")

message(STATUS "---------------------------------------------")
message(STATUS "opentelemetry-cpp build options")
message(STATUS "---------------------------------------------")
message(STATUS "WITH_API_ONLY: ${WITH_API_ONLY}")
message(STATUS "WITH_NO_DEPRECATED_CODE: ${WITH_NO_DEPRECATED_CODE}")
message(STATUS "WITH_ABI_VERSION_1: ${WITH_ABI_VERSION_1}")
message(STATUS "WITH_ABI_VERSION_2: ${WITH_ABI_VERSION_2}")
message(STATUS "OTELCPP_VERSIONED_LIBS: ${OTELCPP_VERSIONED_LIBS}")
message(STATUS "OTELCPP_MAINTAINER_MODE: ${OTELCPP_MAINTAINER_MODE}")
message(STATUS "WITH_STL: ${WITH_STL}")
message(STATUS "WITH_GSL: ${WITH_GSL}")
message(STATUS "WITH_NO_GETENV: ${WITH_NO_GETENV}")

message(STATUS "---------------------------------------------")
message(STATUS "opentelemetry-cpp cmake component options")
message(STATUS "---------------------------------------------")
message(STATUS "WITH_OTLP_GRPC: ${WITH_OTLP_GRPC}")
message(STATUS "WITH_OTLP_HTTP: ${WITH_OTLP_HTTP}")
message(STATUS "WITH_OTLP_FILE: ${WITH_OTLP_FILE}")
message(STATUS "WITH_HTTP_CLIENT_CURL: ${WITH_HTTP_CLIENT_CURL}")
message(STATUS "WITH_ZIPKIN: ${WITH_ZIPKIN}")
message(STATUS "WITH_PROMETHEUS: ${WITH_PROMETHEUS}")
message(STATUS "WITH_ELASTICSEARCH: ${WITH_ELASTICSEARCH}")
message(STATUS "WITH_OPENTRACING: ${WITH_OPENTRACING}")
message(STATUS "WITH_ETW: ${WITH_ETW}")
message(STATUS "OPENTELEMETRY_BUILD_DLL: ${OPENTELEMETRY_BUILD_DLL}")

message(STATUS "---------------------------------------------")
message(STATUS "feature preview options")
message(STATUS "---------------------------------------------")
message(STATUS "WITH_ASYNC_EXPORT_PREVIEW: ${WITH_ASYNC_EXPORT_PREVIEW}")
message(
  STATUS
    "WITH_THREAD_INSTRUMENTATION_PREVIEW: ${WITH_THREAD_INSTRUMENTATION_PREVIEW}"
)
message(
  STATUS "WITH_METRICS_EXEMPLAR_PREVIEW: ${WITH_METRICS_EXEMPLAR_PREVIEW}")
message(STATUS "WITH_REMOVE_METER_PREVIEW: ${WITH_REMOVE_METER_PREVIEW}")
message(
  STATUS "WITH_OTLP_GRPC_SSL_MTLS_PREVIEW: ${WITH_OTLP_GRPC_SSL_MTLS_PREVIEW}")
message(STATUS "WITH_OTLP_RETRY_PREVIEW: ${WITH_OTLP_RETRY_PREVIEW}")
message(STATUS "---------------------------------------------")
message(STATUS "third-party options")
message(STATUS "---------------------------------------------")
message(STATUS "WITH_NLOHMANN_JSON: ${USE_NLOHMANN_JSON}")
message(STATUS "WITH_CURL_LOGGING: ${WITH_CURL_LOGGING}")
message(STATUS "WITH_OTLP_HTTP_COMPRESSION: ${WITH_OTLP_HTTP_COMPRESSION}")
message(STATUS "---------------------------------------------")
message(STATUS "examples and test options")
message(STATUS "---------------------------------------------")
message(STATUS "WITH_BENCHMARK: ${WITH_BENCHMARK}")
message(STATUS "WITH_EXAMPLES: ${WITH_EXAMPLES}")
message(STATUS "WITH_EXAMPLES_HTTP: ${WITH_EXAMPLES_HTTP}")
message(STATUS "WITH_FUNC_TESTS: ${WITH_FUNC_TESTS}")
message(STATUS "BUILD_W3CTRACECONTEXT_TEST: ${BUILD_W3CTRACECONTEXT_TEST}")
message(STATUS "BUILD_TESTING: ${BUILD_TESTING}")
message(STATUS "---------------------------------------------")
message(STATUS "versions")
message(STATUS "---------------------------------------------")
message(STATUS "CMake: ${CMAKE_VERSION}")
message(STATUS "GTest: ${GTest_VERSION}")
message(STATUS "benchmark: ${benchmark_VERSION}")
if(WITH_GSL)
  message(STATUS "GSL: ${GSL_VERSION}")
endif()
if(absl_FOUND)
  message(STATUS "Abseil: ${absl_VERSION}")
endif()
if(Protobuf_FOUND)
  message(STATUS "Protobuf: ${Protobuf_VERSION}")
endif()
if(gRPC_FOUND)
  message(STATUS "gRPC: ${gRPC_VERSION}")
endif()
if(CURL_FOUND)
  message(STATUS "CURL: ${CURL_VERSION}")
endif()
if(ZLIB_FOUND)
  message(STATUS "ZLIB: ${ZLIB_VERSION}")
endif()
if(USE_NLOHMANN_JSON)
  message(STATUS "nlohmann-json: ${nlohmann_json_VERSION}")
endif()
if(prometheus-cpp_FOUND)
  message(STATUS "prometheus-cpp: ${prometheus-cpp_VERSION}")
endif()
message(STATUS "---------------------------------------------")

include("${PROJECT_SOURCE_DIR}/cmake/otel-install-functions.cmake")

include(CMakePackageConfigHelpers)

if(DEFINED OPENTELEMETRY_BUILD_DLL)
  if(NOT WIN32)
    message(FATAL_ERROR "Build DLL is only supported on Windows!")
  endif()
  if(NOT MSVC)
    message(WARNING "Build DLL is supposed to work with MSVC!")
  endif()
  if(WITH_STL)
    message(
      WARNING "Build DLL with C++ STL could cause binary incompatibility!")
  endif()
  add_definitions(-DOPENTELEMETRY_BUILD_EXPORT_DLL)
endif()

add_subdirectory(api)

if(WITH_OPENTRACING)
  find_package(OpenTracing CONFIG QUIET)
  if(NOT OpenTracing_FOUND)
    set(OPENTRACING_DIR "third_party/opentracing-cpp")
    message(STATUS "Trying to use local ${OPENTRACING_DIR} from submodule")
    if(EXISTS "${PROJECT_SOURCE_DIR}/${OPENTRACING_DIR}/.git")
      set(SAVED_BUILD_TESTING ${BUILD_TESTING})
      set(BUILD_TESTING OFF)
      set(SAVED_CMAKE_CXX_INCLUDE_WHAT_YOU_USE
          ${CMAKE_CXX_INCLUDE_WHAT_YOU_USE})
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "")
      add_subdirectory(${OPENTRACING_DIR})
      set(BUILD_TESTING ${SAVED_BUILD_TESTING})
      set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
          ${SAVED_CMAKE_CXX_INCLUDE_WHAT_YOU_USE})
    else()
      message(
        FATAL_ERROR
          "\nopentracing-cpp package was not found. Please either provide it manually or clone with submodules. "
          "To initialize, fetch and checkout any nested submodules, you can use the following command:\n"
          "git submodule update --init --recursive")
    endif()
  else()
    message(STATUS "Using external opentracing-cpp")
  endif()
  add_subdirectory(opentracing-shim)
endif()

if(NOT WITH_API_ONLY)
  set(BUILD_TESTING ${BUILD_TESTING})

  add_subdirectory(sdk)
  add_subdirectory(ext)
  add_subdirectory(exporters)

  if(BUILD_TESTING)
    add_subdirectory(test_common)
  endif()
  if(WITH_EXAMPLES)
    add_subdirectory(examples)
  endif()
  if(WITH_FUNC_TESTS)
    add_subdirectory(functional)
  endif()
endif()

include(cmake/opentelemetry-build-external-component.cmake)
include(cmake/patch-imported-config.cmake)

if(OPENTELEMETRY_INSTALL)
  # Install the cmake config and version files
  otel_install_cmake_config()

  # Install the components and associated files
  otel_install_components()

  # Install the thirdparty dependency definition file
  otel_install_thirdparty_definitions()

  if(BUILD_PACKAGE)
    include(cmake/package.cmake)
    include(CPack)
  endif()
endif()
