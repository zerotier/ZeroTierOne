enable_testing()

if(NOT PostgreSQL_FOUND)
    find_package(PostgreSQL REQUIRED)
endif()

file(GLOB TEST_SOURCES test*.cxx unit/test_*.cxx runner.cxx)

add_executable(runner ${TEST_SOURCES})
target_link_libraries(runner PUBLIC pqxx)
target_include_directories(runner PRIVATE ${PostgreSQL_INCLUDE_DIRS})
add_test(
    NAME runner
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND runner
)

if(INSTALL_TEST)
    install(
        PROGRAMS runner
        TYPE BIN
        RENAME libpqxx-test-runner
    )
endif()
