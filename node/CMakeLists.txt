cmake_minimum_required (VERSION 2.8)
project(zt_core)

if(WIN32)
	add_definitions(-DNOMINMAX)
endif(WIN32)

file(GLOB core_headers *.hpp)
file(GLOB core_src *.cpp)
add_library(${PROJECT_NAME} STATIC ${core_src} ${core_headers})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_11)

if(UNIX) 
	set_source_files_properties(
		Salsa20.cpp
		SHA512.cpp
		C25519.cpp
		Poly1305.cpp
		PROPERTIES
		COMPILE_FLAGS "-Wall -O3"
	)
endif(UNIX)
