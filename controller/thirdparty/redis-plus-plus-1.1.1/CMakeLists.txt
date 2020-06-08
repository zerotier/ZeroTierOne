project(redis++)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    cmake_minimum_required(VERSION 3.0.0)
else()
    cmake_minimum_required(VERSION 2.8.0)
endif()

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -W -Werror -fPIC")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR}/src/sw/redis++)

file(GLOB PROJECT_SOURCE_FILES "${PROJECT_SOURCE_DIR}/*.cpp")

set(STATIC_LIB redispp_static)
# set(SHARED_LIB shared)

add_library(${STATIC_LIB} STATIC ${PROJECT_SOURCE_FILES})
# add_library(${SHARED_LIB} SHARED ${PROJECT_SOURCE_FILES})

# hiredis dependency
find_path(HIREDIS_HEADER hiredis)
target_include_directories(${STATIC_LIB} PUBLIC ${HIREDIS_HEADER})
# target_include_directories(${SHARED_LIB} PUBLIC ${HIREDIS_HEADER})

find_library(HIREDIS_LIB hiredis)
# target_link_libraries(${SHARED_LIB} ${HIREDIS_LIB})

set_target_properties(${STATIC_LIB} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
# set_target_properties(${SHARED_LIB} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

set_target_properties(${STATIC_LIB} PROPERTIES CLEAN_DIRECT_OUTPUT 1)
# set_target_properties(${SHARED_LIB} PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# Install static lib.
install(TARGETS ${STATIC_LIB}
        ARCHIVE DESTINATION lib)

# Install shared lib.
# install(TARGETS ${SHARED_LIB}
#        LIBRARY DESTINATION lib)

#Install headers.
set(HEADER_PATH "sw/redis++")
file(GLOB HEADERS "${PROJECT_SOURCE_DIR}/*.h*")
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${HEADER_PATH})
