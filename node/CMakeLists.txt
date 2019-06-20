cmake_minimum_required (VERSION 2.8)
project(zt_core)

if(WIN32)
	add_definitions(-DNOMINMAX)
endif(WIN32)

file(GLOB core_headers *.hpp)
file(GLOB core_src *.cpp)
add_library(${PROJECT_NAME} STATIC ${core_src} ${core_headers})