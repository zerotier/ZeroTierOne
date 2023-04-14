cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(http-client-lite-examples)

add_executable(${PROJECT_NAME}_simple_request simple_request.cpp)
target_link_libraries(${PROJECT_NAME}_simple_request PRIVATE http_client_lite)
