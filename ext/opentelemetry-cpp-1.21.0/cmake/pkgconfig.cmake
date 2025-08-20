# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Unlike functions, macros do not introduce a scope. This is an advantage when
# trying to set global variables, as we do here.
macro (opentelemetry_set_pkgconfig_paths)
    if (IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
      set(OPENTELEMETRY_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    else ()
      set(OPENTELEMETRY_PC_LIBDIR
            "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
    endif ()

    if (IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
      set(OPENTELEMETRY_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
    else ()
      set(OPENTELEMETRY_PC_INCLUDEDIR
            "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif ()
endmacro ()

# Create the pkgconfig configuration file (aka *.pc files) and the rules to install it.
#
# * library:     the short name of the target, such as `api` or `resources`.
# * name:        the displayed name of the library, such as "OpenTelemetry API".
# * description: the description of the library.
# * ARGN:        the names of any pkgconfig modules the generated module depends on.
#
function (opentelemetry_add_pkgconfig library name description)
  opentelemetry_set_pkgconfig_paths()
  set(target "opentelemetry_${library}")
  set(OPENTELEMETRY_PC_NAME "${name}")
  set(OPENTELEMETRY_PC_DESCRIPTION ${description})
  string(JOIN " " OPENTELEMETRY_PC_REQUIRES ${ARGN})
  get_target_property(target_type ${target} TYPE)
  if ("${target_type}" STREQUAL "INTERFACE_LIBRARY")
    # Interface libraries only contain headers. They do not generate lib files
    # to link against with `-l`.
    set(OPENTELEMETRY_PC_LIBS "")
  else ()
    set(OPENTELEMETRY_PC_LIBS "-l${target}")
  endif ()
  get_target_property(target_defs ${target} INTERFACE_COMPILE_DEFINITIONS)
  if (target_defs)
    foreach (def ${target_defs})
      string(APPEND OPENTELEMETRY_PC_CFLAGS " -D${def}")
    endforeach ()
  endif ()

  # Create and install the pkg-config files.
  configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/config.pc.in" "${target}.pc" @ONLY)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${target}.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endfunction()
