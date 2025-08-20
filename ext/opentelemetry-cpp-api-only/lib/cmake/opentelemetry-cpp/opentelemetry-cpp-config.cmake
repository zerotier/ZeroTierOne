# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

#.rst:
# opentelemetry-cpp-config.cmake
# --------
# Finding opentelemetry-cpp in CMake projects
# ========================================
#
#  - find_package(opentelemetry-cpp CONFIG REQUIRED) to import all installed targets and dependencies
#  - find_package(opentelemetry-cpp CONFIG COMPONENTS ...) to import specific components' targets and dependencies
#
# Example usage
# -------------------
#
# 1. **No Components Specified**
#
#    When no components are provided, all components and their targets are imported.
#
#    .. code-block:: cmake
#
#       find_package(opentelemetry-cpp CONFIG REQUIRED)
#
#
# 2. **Components Specified**
#
#    When a component is specified, its third-party dependencies are found;
#    then, targets from that component, along with the components on which it depends, are imported.
#
#    .. code-block:: cmake
#
#       find_package(opentelemetry-cpp CONFIG REQUIRED COMPONENTS exporters_otlp_grpc)
#
#
#.. note::
#   To troubleshoot issues with ``find_package(opentelemetry-cpp CONFIG REQUIRED)``, run CMake with debug logging enabled:
#
#   .. code-block:: bash
#
#      cmake --log-level=DEBUG -S <source_dir> -B <build_dir>
#
#   To get more verbose output from CMake’s package search, run CMake with the following flag:
#
#   .. code-block:: bash
#
#      cmake -DCMAKE_FIND_DEBUG_MODE=ON -S <source_dir> -B <build_dir>
#
#
# CMake Variables Defined
# --------------------
#
# This module defines the following variables:
#
# ::
#
#   OPENTELEMETRY_CPP_INCLUDE_DIRS  - Include directories of opentelemetry-cpp.
#   OPENTELEMETRY_CPP_LIBRARY_DIRS  - Link directories of opentelemetry-cpp.
#   OPENTELEMETRY_CPP_LIBRARIES     - List of libraries when using opentelemetry-cpp.
#   OPENTELEMETRY_CPP_FOUND         - True if opentelemetry-cpp found.
#   OPENTELEMETRY_ABI_VERSION_NO    - ABI version of opentelemetry-cpp.
#   OPENTELEMETRY_VERSION           - Version of opentelemetry-cpp.
#   OPENTELEMETRY_CPP_COMPONENTS_INSTALLED - List of components installed.
#   opentelemetry-cpp_FOUND         - True if opentelemetry-cpp found.
#   opentelemetry-cpp_<component>_FOUND - True if the requested component is found.
# ::
#
#
# CMake Components and Targets Supported
# --------------------
# opentelemetry-cpp supports the following components and targets. This install may include a subset.
#
# COMPONENTS
#   api
#   sdk
#   ext_common
#   ext_http_curl
#   ext_dll
#   exporters_in_memory
#   exporters_ostream
#   exporters_otlp_common
#   exporters_otlp_file
#   exporters_otlp_grpc
#   exporters_otlp_http
#   exporters_prometheus
#   exporters_elasticsearch
#   exporters_etw
#   exporters_zipkin
#   shims_opentracing
#
# ::
#
# TARGETS
#   opentelemetry-cpp::api                               - Imported target of COMPONENT api
#   opentelemetry-cpp::sdk                               - Imported target of COMPONENT sdk
#   opentelemetry-cpp::version                           - Imported target of COMPONENT sdk
#   opentelemetry-cpp::common                            - Imported target of COMPONENT sdk
#   opentelemetry-cpp::resources                         - Imported target of COMPONENT sdk
#   opentelemetry-cpp::trace                             - Imported target of COMPONENT sdk
#   opentelemetry-cpp::metrics                           - Imported target of COMPONENT sdk
#   opentelemetry-cpp::logs                              - Imported target of COMPONENT sdk
#   opentelemetry-cpp::ext                               - Imported target of COMPONENT ext_common
#   opentelemetry-cpp::http_client_curl                  - Imported target of COMPONENT ext_http_curl
#   opentelemetry-cpp::opentelemetry_cpp                 - Imported target of COMPONENT ext_dll
#   opentelemetry-cpp::in_memory_span_exporter           - Imported target of COMPONENT exporters_in_memory
#   opentelemetry-cpp::in_memory_metric_exporter         - Imported target of COMPONENT exporters_in_memory
#   opentelemetry-cpp::ostream_log_record_exporter       - Imported target of COMPONENT exporters_ostream
#   opentelemetry-cpp::ostream_metrics_exporter          - Imported target of COMPONENT exporters_ostream
#   opentelemetry-cpp::ostream_span_exporter             - Imported target of COMPONENT exporters_ostream
#   opentelemetry-cpp::proto                             - Imported target of COMPONENT exporters_otlp_common
#   opentelemetry-cpp::otlp_recordable                   - Imported target of COMPONENT exporters_otlp_common
#   opentelemetry-cpp::otlp_file_client                  - Imported target of COMPONENT exporters_otlp_file
#   opentelemetry-cpp::otlp_file_exporter                - Imported target of COMPONENT exporters_otlp_file
#   opentelemetry-cpp::otlp_file_log_record_exporter     - Imported target of COMPONENT exporters_otlp_file
#   opentelemetry-cpp::otlp_file_metric_exporter         - Imported target of COMPONENT exporters_otlp_file
#   opentelemetry-cpp::proto_grpc                        - Imported target of COMPONENT exporters_otlp_grpc
#   opentelemetry-cpp::otlp_grpc_client                  - Imported target of COMPONENT exporters_otlp_grpc
#   opentelemetry-cpp::otlp_grpc_exporter                - Imported target of COMPONENT exporters_otlp_grpc
#   opentelemetry-cpp::otlp_grpc_log_record_exporter     - Imported target of COMPONENT exporters_otlp_grpc
#   opentelemetry-cpp::otlp_grpc_metrics_exporter        - Imported target of COMPONENT exporters_otlp_grpc
#   opentelemetry-cpp::otlp_http_client                  - Imported target of COMPONENT exporters_otlp_http
#   opentelemetry-cpp::otlp_http_exporter                - Imported target of COMPONENT exporters_otlp_http
#   opentelemetry-cpp::otlp_http_log_record_exporter     - Imported target of COMPONENT exporters_otlp_http
#   opentelemetry-cpp::otlp_http_metric_exporter         - Imported target of COMPONENT exporters_otlp_http
#   opentelemetry-cpp::prometheus_exporter               - Imported target of COMPONENT exporters_prometheus
#   opentelemetry-cpp::elasticsearch_log_record_exporter - Imported target of COMPONENT exporters_elasticsearch
#   opentelemetry-cpp::etw_exporter                      - Imported target of COMPONENT exporters_etw
#   opentelemetry-cpp::zipkin_trace_exporter             - Imported target of COMPONENT exporters_zipkin
#   opentelemetry-cpp::opentracing_shim                  - Imported target of COMPONENT shims_opentracing
#
# Additional Files Used in Component to Component and Third-Party Dependency Resolution
# --------------------------------------------------------
#
# - **thirdparty-dependency-definitions.cmake**
#   This file lists the third-party dependencies supported by opentelemetry-cpp and components that may require them.
#   Dependencies are found in the order defined in this file when find_package(opentelemetry-cpp …) is invoked.
#
#   **Found using CMake CONFIG search mode:**
#
#   - **absl**
#   - **nlohmann_json**
#   - **Protobuf**
#   - **gRPC**
#   - **prometheus-cpp**
#   - **OpenTracing**
#
#   **Found using the CMake MODULE search mode:**
#
#   - **Threads**
#   - **ZLIB**
#   - **CURL**
#
# - **component-definitions.cmake**
#   This file defines the available opentelemetry-cpp components, the targets associated with each
#   component, and the inter-component dependencies.
#


# =============================================================================
# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
# =============================================================================

set(OPENTELEMETRY_ABI_VERSION_NO
    "1"
    CACHE STRING "opentelemetry-cpp ABI version" FORCE)
set(OPENTELEMETRY_VERSION
    "1.21.0"
    CACHE STRING "opentelemetry-cpp version" FORCE)


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was opentelemetry-cpp-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# Include the opentelemetry-cpp file that includes component defintions, thirdparty definitons and functions to support finding this package and dependencies.
include("${CMAKE_CURRENT_LIST_DIR}/find-package-support-functions.cmake")

set(_INSTALLED_COMPONENTS "")
get_installed_components(_INSTALLED_COMPONENTS)

set(OPENTELEMETRY_CPP_COMPONENTS_INSTALLED ${_INSTALLED_COMPONENTS} CACHE STRING "opentelemetry-cpp components installed" FORCE)

set(_REQUESTED_COMPONENTS "")
get_requested_components(_INSTALLED_COMPONENTS _REQUESTED_COMPONENTS)

find_required_dependencies(_REQUESTED_COMPONENTS)

set_and_check(OPENTELEMETRY_CPP_INCLUDE_DIRS "${PACKAGE_PREFIX_DIR}/include")
set_and_check(OPENTELEMETRY_CPP_LIBRARY_DIRS "${PACKAGE_PREFIX_DIR}/lib")

# include the target files selected and set the component found flag
foreach(_COMPONENT IN LISTS _REQUESTED_COMPONENTS)
  include("${CMAKE_CURRENT_LIST_DIR}/opentelemetry-cpp-${_COMPONENT}-target.cmake")
  set(${CMAKE_FIND_PACKAGE_NAME}_${_COMPONENT}_FOUND TRUE
    CACHE BOOL "whether ${CMAKE_FIND_PACKAGE_NAME} component ${_COMPONENT} is found" FORCE)
endforeach()

# get installed and requested targets
set(_OPENTELEMETRY_CPP_TARGETS "")
get_targets(_REQUESTED_COMPONENTS _OPENTELEMETRY_CPP_TARGETS)
check_targets_imported(_OPENTELEMETRY_CPP_TARGETS)

# Set OPENTELEMETRY_CPP_* variables
set(OPENTELEMETRY_CPP_LIBRARIES)

set(DLL_TARGET "opentelemetry-cpp::opentelemetry_cpp")

foreach(_TARGET IN LISTS _OPENTELEMETRY_CPP_TARGETS)
  if(TARGET "${_TARGET}" AND NOT "${_TARGET}" STREQUAL "${DLL_TARGET}")
    list(APPEND OPENTELEMETRY_CPP_LIBRARIES "${_TARGET}")
  endif()
endforeach()

# handle the QUIETLY and REQUIRED arguments and set opentelemetry-cpp_FOUND to
# TRUE if all variables listed contain valid results, e.g. valid file paths.
include("FindPackageHandleStandardArgs")
find_package_handle_standard_args(
  ${CMAKE_FIND_PACKAGE_NAME}
  FOUND_VAR ${CMAKE_FIND_PACKAGE_NAME}_FOUND
  REQUIRED_VARS OPENTELEMETRY_CPP_INCLUDE_DIRS OPENTELEMETRY_CPP_LIBRARIES)

if(${CMAKE_FIND_PACKAGE_NAME}_FOUND)
  set(OPENTELEMETRY_CPP_FOUND
      ${${CMAKE_FIND_PACKAGE_NAME}_FOUND}
      CACHE BOOL "whether opentelemetry-cpp is found" FORCE)
else()
  unset(OPENTELEMETRY_CPP_FOUND)
  unset(OPENTELEMETRY_CPP_FOUND CACHE)
endif()

check_required_components(${CMAKE_FIND_PACKAGE_NAME})
