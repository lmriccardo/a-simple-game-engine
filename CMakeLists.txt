cmake_minimum_required(VERSION 3.31.0)
project(asge VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(GNUInstallDirs)

option(ASGE_INSTALL "Generate install rules for the ASGE library" ${PROJECT_IS_TOP_LEVEL})

# Search for SDL library
if(WIN32)
    set(SDL3_DIR "${CMAKE_SOURCE_DIR}/third-party/SDL/cmake")
else()
    set(SDL3_DIR "${CMAKE_SOURCE_DIR}/third-party/SDL/lib/cmake/SDL3")
endif()

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

# stb single-header libraries (image loading, font rasterization, ...),
# vendored via scripts/install-stb.* into third-party/stb -- see that script
# for why they're fetched rather than built like SDL. Header-only, so this is
# just an include path; nothing breaks at configure time if they haven't been
# fetched yet, only once code actually includes one of them.
set(ASGE_STB_HEADERS
    stb_image.h
    stb_truetype.h
)

add_library(stb INTERFACE)
add_library(stb::stb ALIAS stb)
target_include_directories(stb INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third-party/stb>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

if(ASGE_INSTALL)
    foreach(stb_header IN LISTS ASGE_STB_HEADERS)
        install(
            FILES "${CMAKE_SOURCE_DIR}/third-party/stb/${stb_header}"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        )
    endforeach()
endif()

add_subdirectory(src/ASGE)

option(ASGE_BUILD_EXAMPLES "Build the examples folder" ON)
if(ASGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(ASGE_INSTALL)
    include(CMakePackageConfigHelpers)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ASGEConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASGEConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ASGEConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ASGE"
    )

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/ASGEConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/ASGEConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ASGE"
    )

    install(
        EXPORT ASGETargets
        FILE ASGETargets.cmake
        NAMESPACE ASGE::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ASGE"
    )
endif()
