cmake_minimum_required(VERSION 2.8)
project(zerotier-root)

if(WIN32)
	add_definitions(-DNOMINMAX)
endif(WIN32)

add_executable(${PROJECT_NAME} root.cpp)

target_link_libraries(${PROJECT_NAME} zt_core zt_osdep pthread resolv)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}/node
	${CMAKE_SOURCE_DIR}/osdep
	${CMAKE_SOURCE_DIR}/ext/json
	${CMAKE_SOURCE_DIR}/ext/cpp-httplib
)
