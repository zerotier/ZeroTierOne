project(prometheus-cpp-simpleapi)
cmake_minimum_required(VERSION 3.2)

add_library               (${PROJECT_NAME} STATIC "./src/simpleapi.cpp" )
target_sources            (${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/prometheus/simpleapi.h")
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries     (${PROJECT_NAME} PUBLIC prometheus-cpp-lite-core)
