# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_LIST_DIR}/component-definitions.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/thirdparty-dependency-definitions.cmake")

#-------------------------------------------------------------------------
# Function to get installed components.
#-------------------------------------------------------------------------
function(get_installed_components installed_components_out)
  set(result "")
  foreach(_COMPONENT IN LISTS OTEL_BUILT_COMPONENTS_LIST)
      set(_COMPONENT_TARGET_FILE "${CMAKE_CURRENT_LIST_DIR}/opentelemetry-cpp-${_COMPONENT}-target.cmake")
      if(EXISTS "${_COMPONENT_TARGET_FILE}")
          list(APPEND result ${_COMPONENT})
          message(DEBUG "get_installed_components: component = ${_COMPONENT},  installed = TRUE")
      else()
          message(DEBUG "get_installed_components: component = ${_COMPONENT},  installed = FALSE")
      endif()
  endforeach()
  set(${installed_components_out} ${result} PARENT_SCOPE)
endfunction()

#-------------------------------------------------------------------------
# Function to get dependent components.
#-------------------------------------------------------------------------
function(get_dependent_components component_in dependent_components_out)
  set(result "")
  set(depends_var "COMPONENT_${component_in}_COMPONENT_DEPENDS")
  if(DEFINED ${depends_var})
    set(result ${${depends_var}})
  endif()
  set(${dependent_components_out} ${result} PARENT_SCOPE)
endfunction()


#-------------------------------------------------------------------------
# Function to get requested components.
#-------------------------------------------------------------------------
function(get_requested_components installed_components_in requested_components_out)
  set(result "")
  if (NOT opentelemetry-cpp_FIND_COMPONENTS)
    set(result ${${installed_components_in}})
    message(DEBUG "get_requested_components: No components explicitly requested. Importing all installed components including: ${result}")
    set(${requested_components_out} ${result} PARENT_SCOPE)
  else()
    message(DEBUG "get_requested_components: Components requested: ${opentelemetry-cpp_FIND_COMPONENTS}")
    foreach(_COMPONENT IN LISTS opentelemetry-cpp_FIND_COMPONENTS)
        if(NOT ${_COMPONENT} IN_LIST OTEL_BUILT_COMPONENTS_LIST)
            message(ERROR " get_requested_components: Component `${_COMPONENT}` is not a built component of the opentelemetry-cpp package. Built components include: ${OTEL_BUILT_COMPONENTS_LIST}")
            return()
        endif()
        if(NOT ${_COMPONENT} IN_LIST ${installed_components_in})
            message(ERROR " get_requested_components: Component `${_COMPONENT}` is supported by opentelemetry-cpp but not installed. Installed components include: ${${installed_components_in}}")
            return()
        endif()
        get_dependent_components(${_COMPONENT} _DEPENDENT_COMPONENTS)
        list(APPEND result ${_DEPENDENT_COMPONENTS})
        list(APPEND result ${_COMPONENT})
    endforeach()
    list(REMOVE_DUPLICATES result)
    set(${requested_components_out} ${result} PARENT_SCOPE)
  endif()
endfunction()


#-------------------------------------------------------------------------
# Function to get the targets for a component.
#-------------------------------------------------------------------------
function(get_component_targets component_in targets_out)
  set(result "")
  if(NOT ${component_in} IN_LIST OTEL_BUILT_COMPONENTS_LIST)
      message(ERROR " get_component_targets: Component `${component_in}` component is not a built component of the opentelemetry-cpp package.")
  else()
    set(targets_var "COMPONENT_${component_in}_TARGETS")
    if(DEFINED ${targets_var})
      set(result ${${targets_var}})
    endif()
  endif()
  set(${targets_out} ${result} PARENT_SCOPE)
endfunction()

#-------------------------------------------------------------------------
# Get targets for a list of components.
#-------------------------------------------------------------------------
function(get_targets components_in targets_out)
  set(result "")
  foreach(_comp IN LISTS ${components_in})
    get_component_targets(${_comp} comp_targets)
    foreach(target IN LISTS comp_targets)
      list(APPEND result ${target})
    endforeach()
  endforeach()
  set(${targets_out} ${result} PARENT_SCOPE)
endfunction()


#-------------------------------------------------------------------------
# Check if a target is imported for a list of targets.
#-------------------------------------------------------------------------
function(check_targets_imported targets_in)
  set(result TRUE)
  foreach(_target IN LISTS ${targets_in})
    if(TARGET ${_target})
      message(DEBUG "check_targets_imported: imported target `${_target}`")
    else()
      message(FATAL_ERROR " check_targets_imported: failed to import target `${_target}`")
      set(result FALSE)
    endif()
  endforeach()
  set(${result_bool_out} ${result} PARENT_SCOPE)
endfunction()

#-------------------------------------------------------------------------
# Check if a dependency is expected and required
#-------------------------------------------------------------------------
function (is_dependency_required dependency_in components_in is_required_out)
  foreach(_component IN LISTS ${components_in})
    if(${dependency_in} IN_LIST COMPONENT_${_component}_THIRDPARTY_DEPENDS)
      set(${is_required_out} TRUE PARENT_SCOPE)
      return()
    endif()
  endforeach()
endfunction()

#-------------------------------------------------------------------------
# Find all required and expected dependencies
#-------------------------------------------------------------------------
include(CMakeFindDependencyMacro)

function(find_required_dependencies components_in)
  foreach(_dependency IN LISTS OTEL_THIRDPARTY_DEPENDENCIES_SUPPORTED)
    if(${_dependency}_FOUND)
      # The dependency is already found by another component. Continue.
      continue()
    endif()
    set(is_required FALSE)
    is_dependency_required(${_dependency} ${components_in} is_required)
    message(DEBUG "find_required_dependencies: dependency = ${_dependency}, is_required = ${is_required}")
    if(is_required)
        message(DEBUG "find_required_dependencies: calling find_dependency(${_dependency} ${OTEL_${_dependency}_SEARCH_MODE} )...")
        find_dependency(${_dependency} ${OTEL_${_dependency}_SEARCH_MODE})
    endif()
  endforeach()
endfunction()