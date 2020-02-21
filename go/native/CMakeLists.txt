cmake_minimum_required(VERSION 2.8)
project(zt_go_native)

set(src
	GoGlue.cpp
)

set(headers
	GoGlue.h
)

add_library(${PROJECT_NAME} STATIC ${src} ${headers})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
