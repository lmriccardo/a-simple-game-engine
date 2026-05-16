cmake_minimum_required(VERSION 3.31.0)
project(asge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Search for SDL library
if(WIN32)
    set(SDL3_DIR "${CMAKE_SOURCE_DIR}/third-party/SDL/cmake")
else()    
    set(SDL3_DIR "${CMAKE_SOURCE_DIR}/third-party/SDL/lib/cmake/SDL3")
endif()
    
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

include(CTest)

add_subdirectory(src/ASGE)

option(ASGE_BUILD_EXAMPLES "Build the examples folder" ON)
if(ASGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
