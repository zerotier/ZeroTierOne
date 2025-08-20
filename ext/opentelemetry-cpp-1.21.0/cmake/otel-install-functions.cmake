# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
include("${PROJECT_SOURCE_DIR}/cmake/thirdparty-dependency-config.cmake")

########################################################################
# INTERNAL FUNCTIONS - do not call directly. Use the otel_* "Main" functions
########################################################################

#-----------------------------------------------------------------------
# _otel_set_component_properties:
#   Sets the component properties used for install.
#   Properties set on PROJECT_SOURCE_DIR directory include:
#     OTEL_COMPONENTS_LIST: List of components added using otel_add_component
#     OTEL_COMPONENT_TARGETS_<component>: List of targets associated with the component
#     OTEL_COMPONENT_TARGETS_ALIAS_<component>: List of targets aliases associated with the component
#     OTEL_COMPONENT_FILES_DIRECTORY_<component>: Directory containing files to be installed with the component
#     OTEL_COMPONENT_FILES_DESTINATION_<component>: Destination directory for the files
#     OTEL_COMPONENT_FILES_MATCHING_<component>: Matching pattern for the files to be installed
#     OTEL_COMPONENT_DEPENDS_<component>: List of components that this component depends on
#     OTEL_COMPONENT_THIRDPARTY_DEPENDS_<component>: List of thirdparty dependencies that this component depends on
#-----------------------------------------------------------------------
function(_otel_set_component_properties)
    set(optionArgs )
    set(oneValueArgs COMPONENT FILES_DIRECTORY FILES_DESTINATION)
    set(multiValueArgs TARGETS TARGETS_ALIAS FILES_MATCHING COMPONENT_DEPENDS THIRDPARTY_DEPENDS)
    cmake_parse_arguments(_PROPERTIES "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" "${ARGN}")

    # Add the component to the current components list
    get_property(existing_components DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENTS_LIST)
    if(_PROPERTIES_COMPONENT IN_LIST existing_components)
      message(FATAL_ERROR "  component ${_PROPERTIES_COMPONENT} has already been created.")
    endif()
    list(APPEND existing_components "${_PROPERTIES_COMPONENT}")
    set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENTS_LIST "${existing_components}")

    # Set the component targets property
    if(_PROPERTIES_TARGETS)
      set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_TARGETS_${_PROPERTIES_COMPONENT} "${_PROPERTIES_TARGETS}")
    else()
      message(FATAL_ERROR "  component ${_PROPERTIES_COMPONENT} does not have any targets.")
    endif()

    # Set the component targets alias property
    if(_PROPERTIES_TARGETS_ALIAS)
      set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_TARGETS_ALIAS_${_PROPERTIES_COMPONENT} "${_PROPERTIES_TARGETS_ALIAS}")
    endif()

    # Set the component files property
    if(_PROPERTIES_FILES_DIRECTORY)
      if(NOT _PROPERTIES_FILES_DESTINATION)
        message(FATAL_ERROR "  component ${_PROPERTIES_COMPONENT} has FILES_DIRECTORY set and must have FILES_DESINATION set.")
      endif()
      if(NOT _PROPERTIES_FILES_MATCHING)
        message(FATAL_ERROR "  component ${_PROPERTIES_COMPONENT} has FILES_DIRECTORY set and must have FILES_MATCHING set.")
      endif()

      set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_FILES_DIRECTORY_${_PROPERTIES_COMPONENT} "${_PROPERTIES_FILES_DIRECTORY}")
      set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_FILES_DESTINATION_${_PROPERTIES_COMPONENT} "${_PROPERTIES_FILES_DESTINATION}")
      set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_FILES_MATCHING_${_PROPERTIES_COMPONENT} "${_PROPERTIES_FILES_MATCHING}")
    endif()

    if(_PROPERTIES_COMPONENT_DEPENDS)
      set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_DEPENDS_${_PROPERTIES_COMPONENT} "${_PROPERTIES_COMPONENT_DEPENDS}")
    endif()

    if(_PROPERTIES_THIRDPARTY_DEPENDS)
      set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_THIRDPARTY_DEPENDS_${_PROPERTIES_COMPONENT} "${_PROPERTIES_THIRDPARTY_DEPENDS}")
    endif()
endfunction()

#-----------------------------------------------------------------------
# _otel_set_target_component_property:
#   Sets the target's INTERFACE_OTEL_COMPONENT_NAME property to the component.
#   A target can only be assigned to one component.
# Note: The INTERFACE_* prefix can be dropped with CMake 3.19+ when custom
#       properties without the prefix are supported on INTERFACE targets.
#-----------------------------------------------------------------------
function(_otel_set_target_component_property _TARGET _COMPONENT)
  get_target_property(_TARGET_COMPONENT ${_TARGET} INTERFACE_OTEL_COMPONENT_NAME)
  if(_TARGET_COMPONENT)
    message(FATAL_ERROR "  Target ${_TARGET} is already assigned to an opentelemetry-cpp COMPONENT ${_TARGET_COMPONENT}.")
  endif()
  set_target_properties(${_TARGET} PROPERTIES INTERFACE_OTEL_COMPONENT_NAME ${_OTEL_ADD_COMP_COMPONENT})
endfunction()

#-----------------------------------------------------------------------
# _otel_append_dependent_components:
#   Appends the dependent component to the OUT_COMPONENTS variable.
#   The dependent component is defined in the OTEL_COMPONENT_DEPENDS_<component> property
#   on the PROJECT_SOURCE_DIR directory.
#-----------------------------------------------------------------------
function(_otel_append_dependent_components _COMPONENT OUT_COMPONENTS)
  get_property(_COMPONENT_DEPENDS DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_DEPENDS_${_COMPONENT})
  if(_COMPONENT_DEPENDS)
    set(_output_components "${${OUT_COMPONENTS}}")
    message(DEBUG "  - adding dependent component ${_COMPONENT_DEPENDS} from component ${_COMPONENT}")
    list(APPEND _output_components ${_COMPONENT_DEPENDS})
    set(${OUT_COMPONENTS} "${_output_components}" PARENT_SCOPE)
  endif()
endfunction()

#-----------------------------------------------------------------------
# _otel_append_component_found:
#   Checks if the target is associated with an otel component using the INTERFACE_OTEL_COMPONENT_NAME target property.
#   If so then the component is appended to the OUT_COMPONENTS varaiable.
#-----------------------------------------------------------------------
function(_otel_append_component_found _COMPONENT _TARGET OUT_COMPONENTS OUT_COMPONENT_FOUND)
  set(_output_components "${${OUT_COMPONENTS}}")
  get_target_property(_DEPEND_COMPONENT ${_TARGET} INTERFACE_OTEL_COMPONENT_NAME)
  if(_DEPEND_COMPONENT AND NOT ${_DEPEND_COMPONENT} STREQUAL ${_COMPONENT})
    _otel_append_dependent_components(${_DEPEND_COMPONENT} _output_components)
    message(DEBUG "  - adding dependent component ${_DEPEND_COMPONENT} from target ${_TARGET}")
    list(APPEND _output_components ${_DEPEND_COMPONENT})
    set(${OUT_COMPONENT_FOUND} TRUE PARENT_SCOPE)
  else()
    set(${OUT_COMPONENT_FOUND} FALSE PARENT_SCOPE)
  endif()
  set(${OUT_COMPONENTS} "${_output_components}" PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------
# _otel_append_thirdparty_found:
#   Tries to match one of the supported third-party dependencies to the target name.
#   If found the dependency project name is appended to the OUT_THIRDPARTY_DEPS variable.
#   The match is based on the OTEL_THIRDPARTY_DEPENDENCIES_SUPPORTED list and optional
#   OTEL_<dependency>_TARGET_NAMESPACE variables.
#------------------------------------------------------------------------
function(_otel_append_thirdparty_found _TARGET OUT_THIRDPARTY_DEPS)
  set(_output_thirdparty_deps "${${OUT_THIRDPARTY_DEPS}}")

  foreach(_DEPENDENCY ${OTEL_THIRDPARTY_DEPENDENCIES_SUPPORTED})
    # Search for the dependency namespace in the target name
    if(NOT "${OTEL_${_DEPENDENCY}_TARGET_NAMESPACE}" STREQUAL "")
      set(_DEPENDENCY_NAMESPACE "${OTEL_${_DEPENDENCY}_TARGET_NAMESPACE}")
    else()
      set(_DEPENDENCY_NAMESPACE "${_DEPENDENCY}")
    endif()
    string(FIND "${_TARGET}" "${_DEPENDENCY_NAMESPACE}" _is_thirdparty)
    if(_is_thirdparty GREATER -1)
      message(DEBUG "  - adding thirdparty dependency ${_DEPENDENCY} from target ${_TARGET}")
      list(APPEND _output_thirdparty_deps ${_DEPENDENCY})
    endif()
  endforeach()
  set(${OUT_THIRDPARTY_DEPS} "${_output_thirdparty_deps}" PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------
# _otel_collect_component_dependencies:
#   Collects the component to component dependencies and thirdparty dependencies of a target.
#   The dependencies are collected from the target's LINK_LIBRARIES property# and are appended
#   to the OUT_COMPONENT_DEPS and OUT_THIRDPARTY_DEPS variables.
#------------------------------------------------------------------------
function(_otel_collect_component_dependencies _TARGET _COMPONENT OUT_COMPONENT_DEPS OUT_THIRDPARTY_DEPS)
  get_target_property(_TARGET_TYPE ${_TARGET} TYPE)
  message(DEBUG "  Target: ${_TARGET} - Type: ${_TARGET_TYPE}")

  # Set the linked libraries to search for dependencies
  set(_linked_libraries "")
  if(_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
    get_target_property(_interface_libs ${_TARGET} INTERFACE_LINK_LIBRARIES)
    set(_linked_libraries "${_interface_libs}")
    message(DEBUG "  - INTERFACE_LINK_LIBRARIES: ${_interface_libs}")
  else()
    get_target_property(_link_libs ${_TARGET} LINK_LIBRARIES)
    set(_linked_libraries "${_link_libs}")
    message(DEBUG "  - LINK_LIBRARIES: ${_link_libs}")
  endif()

  set(_component_deps "${${OUT_COMPONENT_DEPS}}")
  set(_thirdparty_deps "${${OUT_THIRDPARTY_DEPS}}")

  foreach(_linked_target ${_linked_libraries})
    # Handle targets
    if(TARGET "${_linked_target}")
      set(_component_found FALSE)
      _otel_append_component_found(${_COMPONENT} "${_linked_target}" _component_deps _component_found)
      if(NOT ${_component_found})
        _otel_append_thirdparty_found(${_linked_target} _thirdparty_deps)
      endif()
      continue()
    endif()

    # Skip BUILD_INTERFACE targets
    string(FIND "${_linked_target}" "$<BUILD_INTERFACE:" _is_build_interface)
    if(_is_build_interface GREATER -1)
      message(DEBUG "  - skipping BUILD_INTERFACE target: ${_linked_target}")
      continue()
    endif()

    # Handle targets in generator expressions
    string(FIND "${_linked_target}" "$<" _is_generator_expression)
    if(_is_generator_expression GREATER -1)
      # Find targets in generator expressions (there can be more than one per expression)
      string(REGEX MATCHALL "[A-Za-z0-9_\\-\\.]+(::[A-Za-z0-9_\\-\\.]+)*" _parsed_targets "${_linked_target}")
      foreach(_parsed_target ${_parsed_targets})
        if(TARGET ${_parsed_target})
          set(_component_found FALSE)
          _otel_append_component_found(${_COMPONENT} "${_parsed_target}" _component_deps _component_found)
          if(NOT ${_component_found})
            _otel_append_thirdparty_found(${_parsed_target} _thirdparty_deps)
          endif()
        endif()
      endforeach()
    endif()
  endforeach()

  list(REMOVE_DUPLICATES _component_deps)
  list(REMOVE_DUPLICATES _thirdparty_deps)

  set(${OUT_COMPONENT_DEPS} "${_component_deps}" PARENT_SCOPE)
  set(${OUT_THIRDPARTY_DEPS} "${_thirdparty_deps}" PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------
# _otel_add_target_alias:
#   Adds an alias target using target's export name and project name.
#   opentelemetry-cpp::<target_export_name>
#------------------------------------------------------------------------
function(_otel_add_target_alias _TARGET OUT_ALIAS_TARGETS)
  get_target_property(_TARGET_EXPORT_NAME ${_TARGET} EXPORT_NAME)
  if(NOT _TARGET_EXPORT_NAME)
    message(FATAL_ERROR "  Target ${_TARGET} does not have an EXPORT_NAME property.")
  elseif(NOT TARGET "${PROJECT_NAME}::${_TARGET_EXPORT_NAME}")
    add_library("${PROJECT_NAME}::${_TARGET_EXPORT_NAME}" ALIAS ${_TARGET})
  endif()
  set(_alias_targets "${${OUT_ALIAS_TARGETS}}")
  list(APPEND _alias_targets "${PROJECT_NAME}::${_TARGET_EXPORT_NAME}")
  set(${OUT_ALIAS_TARGETS} "${_alias_targets}" PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------
# _otel_install_component:
#   Installs the component targets and optional files
#-----------------------------------------------------------------------
function(_otel_install_component _COMPONENT)
  install(
    TARGETS ${_COMPONENT_TARGETS}
    EXPORT "${PROJECT_NAME}-${_COMPONENT}-target"
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${_COMPONENT})

  install(
    EXPORT "${PROJECT_NAME}-${_COMPONENT}-target"
    FILE "${PROJECT_NAME}-${_COMPONENT}-target.cmake"
    NAMESPACE "${PROJECT_NAME}::"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    COMPONENT ${_COMPONENT})

  if(_COMPONENT_FILES_DIRECTORY)
    install(
      DIRECTORY ${_COMPONENT_FILES_DIRECTORY}
      DESTINATION ${_COMPONENT_FILES_DESTINATION}
      COMPONENT ${_COMPONENT}
      FILES_MATCHING ${_COMPONENT_FILES_MATCHING})
  endif()
endfunction()

#-----------------------------------------------------------------------
# _otel_populate_component_targets_block:
#   Populates the OTEL_COMPONENTS_TARGETS_BLOCK with the component targets
#   - sets COMPONENT_<component>_TARGETS variables
#-----------------------------------------------------------------------
function(_otel_populate_component_targets_block IN_COMPONENT COMPONENTS_TARGETS_BLOCK)
  # Populate OTEL_COMPONENTS_TARGETS_BLOCK
  set(_targets_block ${${COMPONENTS_TARGETS_BLOCK}})
  string(APPEND _targets_block
    "# COMPONENT ${IN_COMPONENT}\n"
    "set(COMPONENT_${IN_COMPONENT}_TARGETS\n"
  )
  foreach(_TARGET IN LISTS _COMPONENT_TARGETS_ALIAS)
    string(APPEND _targets_block "    ${_TARGET}\n")
  endforeach()
  string(APPEND _targets_block ")\n\n")
  set(${COMPONENTS_TARGETS_BLOCK} "${_targets_block}" PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------
# _otel_populate_component_internal_depends_block:
#   Populates the OTEL_COMPONENTS_INTERNAL_DEPENDENCIES_BLOCK with the component dependencies
#   - sets COMPONENT_<component>_COMPONENT_DEPENDS variables
#-----------------------------------------------------------------------
function(_otel_populate_component_internal_depends_block IN_COMPONENT COMPONENTS_INTERNAL_DEPENDENCIES_BLOCK)
  # Populate OTEL_COMPONENTS_INTERNAL_DEPENDENCIES_BLOCK
  set(_deps_block ${${COMPONENTS_INTERNAL_DEPENDENCIES_BLOCK}})
  string(APPEND _deps_block
      "# COMPONENT ${IN_COMPONENT} internal dependencies\n"
      "set(COMPONENT_${IN_COMPONENT}_COMPONENT_DEPENDS\n"
    )
  foreach(dep IN LISTS _COMPONENT_DEPENDS)
    string(APPEND _deps_block "    ${dep}\n")
  endforeach()
  string(APPEND _deps_block ")\n\n")
  set(${COMPONENTS_INTERNAL_DEPENDENCIES_BLOCK} "${_deps_block}" PARENT_SCOPE)
endfunction()

#-----------------------------------------------------------------------
function(_otel_populate_component_thirdparty_depends_block IN_COMPONENT COMPONENTS_THIRDPARTY_DEPENDENCIES_BLOCK)
  # Populate OTEL_COMPONENTS_THIRDPARTY_DEPENDENCIES_BLOCK
  set(_deps_block ${${COMPONENTS_THIRDPARTY_DEPENDENCIES_BLOCK}})
  string(APPEND _deps_block
      "# COMPONENT ${IN_COMPONENT} thirdparty dependencies\n"
      "set(COMPONENT_${IN_COMPONENT}_THIRDPARTY_DEPENDS\n"
    )
  foreach(dep IN LISTS _COMPONENT_THIRDPARTY_DEPENDS)
    string(APPEND _deps_block "    ${dep}\n")
  endforeach()
  string(APPEND _deps_block ")\n\n")
  set(${COMPONENTS_THIRDPARTY_DEPENDENCIES_BLOCK} "${_deps_block}" PARENT_SCOPE)
endfunction()
#-----------------------------------------------------------------------

########################################################################
# Main functions to support installing components
# and the opentlemetry-cpp cmake package config files
########################################################################

#-----------------------------------------------------------------------
# otel_add_component:
#   Adds a component to the list of components to be installed. A component name and list of targest are required.
#   Optional files can be added to the component by specifying a directory, destination and matching pattern.
#   Each target is assigned to the component and its dependencies are identified based on the LINK_LIBRARIES property.
#   An alias target is also created for each target in the form of PROJECT_NAME::TARGET_EXPORT_NAME.
# Usage:
#    otel_add_component(
#      COMPONENT <component_name>
#      TARGETS <target1> <target2> ...
#      FILES_DIRECTORY <directory>
#      FILES_DESTINATION <destination>
#      FILES_MATCHING <matching>)
#-----------------------------------------------------------------------
function(otel_add_component)
  set(optionArgs )
  set(oneValueArgs COMPONENT FILES_DIRECTORY FILES_DESTINATION)
  set(multiValueArgs TARGETS FILES_MATCHING)
  cmake_parse_arguments(_OTEL_ADD_COMP "${optionArgs}" "${oneValueArgs}" "${multiValueArgs}" "${ARGN}")

  if(NOT _OTEL_ADD_COMP_COMPONENT)
    message(FATAL_ERROR "otel_add_component: COMPONENT is required")
  endif()

  if(NOT _OTEL_ADD_COMP_TARGETS)
    message(FATAL_ERROR "otel_add_component: TARGETS is required")
  endif()

  message(DEBUG "Add COMPONENT: ${_OTEL_ADD_COMP_COMPONENT}")
  set(_COMPONENT_DEPENDS "")
  set(_THIRDPARTY_DEPENDS "")
  set(_ALIAS_TARGETS "")

  foreach(_TARGET ${_OTEL_ADD_COMP_TARGETS})
    if(NOT TARGET ${_TARGET})
      message(FATAL_ERROR "  Target ${_TARGET} not found")
    endif()
    _otel_set_target_component_property(${_TARGET} ${_OTEL_ADD_COMP_COMPONENT})
    _otel_collect_component_dependencies(${_TARGET} ${_OTEL_ADD_COMP_COMPONENT} _COMPONENT_DEPENDS _THIRDPARTY_DEPENDS)
    _otel_add_target_alias(${_TARGET} _ALIAS_TARGETS)
  endforeach()

  if(_OTEL_ADD_COMP_FILES_DIRECTORY)
    set(_OTEL_ADD_COMP_FILES_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_OTEL_ADD_COMP_FILES_DIRECTORY}")
  endif()

  message(DEBUG "  TARGETS: ${_OTEL_ADD_COMP_TARGETS}")
  message(DEBUG "  TARGETS_ALIAS: ${_ALIAS_TARGETS}")
  message(DEBUG "  COMPONENT_DEPENDS: ${_COMPONENT_DEPENDS}")
  message(DEBUG "  THIRDPARTY_DEPENDS: ${_THIRDPARTY_DEPENDS}")
  message(DEBUG "  FILES_DIRECTORY: ${_OTEL_ADD_COMP_FILES_DIRECTORY}")
  message(DEBUG "  FILES_DESTINATION: ${_OTEL_ADD_COMP_FILES_DESTINATION}")
  message(DEBUG "  FILES_MATCHING: ${_OTEL_ADD_COMP_FILES_MATCHING}")

  _otel_set_component_properties(
    COMPONENT ${_OTEL_ADD_COMP_COMPONENT}
    TARGETS ${_OTEL_ADD_COMP_TARGETS}
    TARGETS_ALIAS ${_ALIAS_TARGETS}
    FILES_DIRECTORY ${_OTEL_ADD_COMP_FILES_DIRECTORY}
    FILES_DESTINATION ${_OTEL_ADD_COMP_FILES_DESTINATION}
    FILES_MATCHING ${_OTEL_ADD_COMP_FILES_MATCHING}
    COMPONENT_DEPENDS ${_COMPONENT_DEPENDS}
    THIRDPARTY_DEPENDS ${_THIRDPARTY_DEPENDS})
endfunction()

#-----------------------------------------------------------------------
# otel_install_components:
#   Installs all components that have been added using otel_add_component.
#   The components are installed in the order they were added.
#   The install function will create a cmake config file for each component
#   that contains the component name, targets, dependencies and thirdparty dependencies.
# Usage:
#    otel_install_components()
#-----------------------------------------------------------------------
function(otel_install_components)
  get_property(OTEL_BUILT_COMPONENTS_LIST DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENTS_LIST)
  message(STATUS "Installing components:")
  set(OTEL_COMPONENTS_TARGETS_BLOCK "")
  set(OTEL_COMPONENTS_INTERNAL_DEPENDENCIES_BLOCK "")
  set(OTEL_COMPONENTS_THIRDPARTY_DEPENDENCIES_BLOCK "")

  foreach(_COMPONENT ${OTEL_BUILT_COMPONENTS_LIST})
    get_property(_COMPONENT_DEPENDS DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_DEPENDS_${_COMPONENT})
    get_property(_COMPONENT_TARGETS DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_TARGETS_${_COMPONENT})
    get_property(_COMPONENT_TARGETS_ALIAS DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_TARGETS_ALIAS_${_COMPONENT})
    get_property(_COMPONENT_THIRDPARTY_DEPENDS DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_THIRDPARTY_DEPENDS_${_COMPONENT})
    get_property(_COMPONENT_FILES_DIRECTORY DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_FILES_DIRECTORY_${_COMPONENT})
    get_property(_COMPONENT_FILES_DESTINATION DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_FILES_DESTINATION_${_COMPONENT})
    get_property(_COMPONENT_FILES_MATCHING DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY OTEL_COMPONENT_FILES_MATCHING_${_COMPONENT})

    message(STATUS "Install COMPONENT ${_COMPONENT}")
    message(STATUS "  TARGETS: ${_COMPONENT_TARGETS}")
    message(STATUS "  TARGETS_ALIAS: ${_COMPONENT_TARGETS_ALIAS}")
    message(STATUS "  COMPONENT_DEPENDS: ${_COMPONENT_DEPENDS}")
    message(STATUS "  THIRDPARTY_DEPENDS: ${_COMPONENT_THIRDPARTY_DEPENDS}")
    message(STATUS "  FILES_DIRECTORY: ${_COMPONENT_FILES_DIRECTORY}")
    message(STATUS "  FILES_DESTINATION: ${_COMPONENT_FILES_DESTINATION}")
    message(STATUS "  FILES_MATCHING: ${_COMPONENT_FILES_MATCHING}")

    _otel_install_component(${_COMPONENT})
    _otel_populate_component_targets_block(${_COMPONENT} OTEL_COMPONENTS_TARGETS_BLOCK)
    _otel_populate_component_internal_depends_block(${_COMPONENT} OTEL_COMPONENTS_INTERNAL_DEPENDENCIES_BLOCK)
    _otel_populate_component_thirdparty_depends_block(${_COMPONENT} OTEL_COMPONENTS_THIRDPARTY_DEPENDENCIES_BLOCK)
  endforeach()

  configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/templates/component-definitions.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/component-definitions.cmake"
    @ONLY
  )

  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/cmake/component-definitions.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    COMPONENT cmake-config)
endfunction()

#-----------------------------------------------------------------------
# otel_install_thirdparty_definitions:
#   Installs the thirdparty dependency definitions file that contains the list
#   of thirdparty dependencies their versions and cmake search modes.
#   - sets `OTEL_THIRDPARTY_DEPENDENCIES_SUPPORTED` to the list of dependencies
#   - sets `OTEL_<dependency>_VERSION` to the version used to build opentelemetry-cpp
#   - sets `OTEL_<dependency>_SEARCH_MODE` to the search mode required to find the dependency
# Usage:
#    otel_install_thirdparty_definitions()
#-----------------------------------------------------------------------
function(otel_install_thirdparty_definitions)
  set(OTEL_THIRDPARTY_DEPENDENCY_VERSIONS_BLOCK "")
  set(OTEL_THIRDPARTY_DEPENDENCY_SEARCH_MODES_BLOCK "")

  # Populate OTEL_THIRDPARTY_DEPENDENCY_VERSIONS_BLOCK
  foreach(_DEPENDENCY ${OTEL_THIRDPARTY_DEPENDENCIES_SUPPORTED})
    string(APPEND OTEL_THIRDPARTY_DEPENDENCY_VERSIONS_BLOCK
      "set(OTEL_${_DEPENDENCY}_VERSION \"${${_DEPENDENCY}_VERSION}\")\n"
    )
  endforeach()

  # Populate OTEL_THIRDPARTY_DEPENDENCY_SEARCH_MODES_BLOCK
  foreach(_DEPENDENCY ${OTEL_THIRDPARTY_DEPENDENCIES_SUPPORTED})
    string(APPEND OTEL_THIRDPARTY_DEPENDENCY_SEARCH_MODES_BLOCK
      "set(OTEL_${_DEPENDENCY}_SEARCH_MODE \"${OTEL_${_DEPENDENCY}_SEARCH_MODE}\")\n"
    )
  endforeach()

  configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/templates/thirdparty-dependency-definitions.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/thirdparty-dependency-definitions.cmake"
    @ONLY)

  install(
      FILES
      "${CMAKE_CURRENT_BINARY_DIR}/cmake/thirdparty-dependency-definitions.cmake"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
      COMPONENT cmake-config)
endfunction()

#-----------------------------------------------------------------------
# otel_install_cmake_config:
#   Configures and installs the cmake.config package file and version file
#   to support find_package(opentelemetry-cpp CONFIG COMPONENTS ...)
# Usage:
#    otel_install_cmake_config()
#-----------------------------------------------------------------------
function(otel_install_cmake_config)
  # Write config file for find_package(opentelemetry-cpp CONFIG)
  set(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
  configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/templates/opentelemetry-cpp-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    PATH_VARS OPENTELEMETRY_ABI_VERSION_NO OPENTELEMETRY_VERSION PROJECT_NAME
              INCLUDE_INSTALL_DIR CMAKE_INSTALL_LIBDIR)

  # Write version file for find_package(opentelemetry-cpp CONFIG)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake"
    VERSION ${OPENTELEMETRY_VERSION}
    COMPATIBILITY ExactVersion)

  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}/${PROJECT_NAME}-config-version.cmake"
      "${CMAKE_CURRENT_LIST_DIR}/cmake/find-package-support-functions.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    COMPONENT cmake-config)
endfunction()