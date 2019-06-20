include_directories(${LIBRABBITMQ_INCLUDE_DIRS})

if (MSVC)
  # No version of MSVC has inttypes.h, this uses the msinttypes
  #  Note this includes stdint.h which is either in 
  #  ../librabbitmq/win32/msinttypes or in a standard location
  include_directories(win32/msinttypes)
endif (MSVC)

add_definitions(-DHAVE_CONFIG_H)
add_definitions(-DAMQP_STATIC)

add_executable(test_parse_url test_parse_url.c)
target_link_libraries(test_parse_url rabbitmq-static)
add_test(parse_url test_parse_url)

add_executable(test_tables test_tables.c)
target_link_libraries(test_tables rabbitmq-static)
add_test(tables test_tables)
configure_file(test_tables.expected ${CMAKE_CURRENT_BINARY_DIR}/tests/test_tables.expected COPYONLY)

add_executable(test_hostcheck
               test_hostcheck.c
               ../librabbitmq/amqp_hostcheck.c)
add_test(hostcheck test_hostcheck)

add_executable(test_status_enum
               test_status_enum.c)
target_link_libraries(test_status_enum rabbitmq-static)
add_test(status_enum test_status_enum)

add_executable(test_basic
               test_basic.c)
target_link_libraries(test_basic rabbitmq-static)

if (NOT APPLE)
  add_test(basic test_basic)
endif()

add_executable(test_sasl_mechanism test_sasl_mechanism.c)
target_link_libraries(test_sasl_mechanism rabbitmq-static)
add_test(sasl_mechanism test_sasl_mechanism)

add_executable(test_merge_capabilities test_merge_capabilities.c)
target_link_libraries(test_merge_capabilities rabbitmq-static)
add_test(merge_capabilities test_merge_capabilities)

