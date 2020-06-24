cmake_minimum_required(VERSION 3.0)
project(zt_service_io_core)

set(src
	GoGlue.cpp
)

set(headers
	GoGlue.h
)

add_library(${PROJECT_NAME} STATIC ${src} ${headers})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)
target_include_directories(
	${PROJECT_NAME}
	PUBLIC
	${CMAKE_BINARY_DIR}/core
)