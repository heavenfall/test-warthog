cmake_minimum_required(VERSION 3.13)

project(App
	VERSION 0.0.1
	LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# warthog modules
include(cmake/warthog.cmake)

warthog_module_declare(warthog-core) # is optional, remove for default of main
warthog_module(warthog-core)

add_executable(hash src/hash_table.cpp)
target_link_libraries(hash PUBLIC warthog::core)
