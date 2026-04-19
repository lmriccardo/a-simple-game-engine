cmake_minimum_required(VERSION 3.31.0)
project(asge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Search for SDL library
set(SDL3_DIR "${CMAKE_SOURCE_DIR}/third-party/SDL/cmake")
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

add_subdirectory(src/ASGE)
add_executable(asge src/main.cpp)
target_link_libraries(asge PRIVATE ASGE)
set_target_properties(asge
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

if(WIN32)
    add_custom_command(TARGET asge POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:asge>
    )
endif()