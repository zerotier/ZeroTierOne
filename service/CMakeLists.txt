cmake_minimum_required(VERSION 2.8)
project(zt_service)

if(WIN32)
	add_definitions(-DNOMINMAX)
endif(WIN32)

set(src
	OneService.cpp
)

set(headers
	OneService.hpp
)

add_library(${PROJECT_NAME} STATIC ${src} ${headers})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
