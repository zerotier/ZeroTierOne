project(prometheus-cpp-lite-core)
cmake_minimum_required(VERSION 3.2)

file(GLOB_RECURSE PROMETHEUS_CPP_LITE_HEADERS *.h)

# it is header only target

add_library               (${PROJECT_NAME}     INTERFACE)
target_sources            (${PROJECT_NAME}     INTERFACE ${PROMETHEUS_CPP_LITE_HEADERS})
target_include_directories(${PROJECT_NAME}     INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_custom_target         (${PROJECT_NAME}-ide SOURCES   ${PROMETHEUS_CPP_LITE_HEADERS})
target_link_libraries     (${PROJECT_NAME}     INTERFACE http-client-lite)

set                       (${PROJECT_NAME}_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)

# it need for save_to_file_t
if(NOT WIN32)
  find_package(Threads)
  target_link_libraries(${PROJECT_NAME} INTERFACE ${CMAKE_THREAD_LIBS_INIT})
endif()
