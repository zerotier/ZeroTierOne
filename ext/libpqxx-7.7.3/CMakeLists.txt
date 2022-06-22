cmake_minimum_required(VERSION 3.8)

file(READ VERSION VER_FILE_CONTENT)
string(STRIP ${VER_FILE_CONTENT} VER_FILE_CONTENT)

project(
    libpqxx
    VERSION ${VER_FILE_CONTENT}
    LANGUAGES CXX
)

if(NOT "${CMAKE_CXX_STANDARD}")
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

option(BUILD_DOC "Build documentation" OFF)

if(NOT SKIP_BUILD_TEST)
    option(BUILD_TEST "Build all test cases" ON)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(config)

add_subdirectory(src)
add_subdirectory(include)
if(BUILD_DOC)
    add_subdirectory(doc)
endif()
if(BUILD_TEST)
    add_subdirectory(test)
endif()

# installation
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/libpqxx-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES cmake/libpqxx-config.cmake
              "${CMAKE_CURRENT_BINARY_DIR}/libpqxx-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libpqxx
)
install(
    EXPORT libpqxx-targets
    NAMESPACE libpqxx::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libpqxx
)
# Build tree export
export(
    EXPORT libpqxx-targets
    NAMESPACE libpqxx::
    FILE ${CMAKE_CURRENT_BINARY_DIR}/libpqxx-targets.cmake
)
configure_file(
    cmake/libpqxx-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/libpqxx-config.cmake
    COPYONLY
)
# Package generation
set(CPACK_GENERATOR TGZ)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
include(CPack)
