if(NOT PostgreSQL_FOUND)
    if(POLICY CMP0074)
        cmake_policy(PUSH)
        # CMP0074 is `OLD` by `cmake_minimum_required(VERSION 3.7)`, sets `NEW`
        # to enable support CMake variable `PostgreSQL_ROOT`.
        cmake_policy(SET CMP0074 NEW)
    endif()

    find_package(PostgreSQL REQUIRED)

    if(POLICY CMP0074)
        cmake_policy(POP)
    endif()
endif()

file(GLOB UNIT_TEST_SOURCES *.cxx)

add_executable(unit_runner ${UNIT_TEST_SOURCES})
target_link_libraries(unit_runner PUBLIC pqxx)
target_include_directories(unit_runner PRIVATE ${PostgreSQL_INCLUDE_DIRS})
add_test(
    NAME unit_runner
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND unit_runner
)
