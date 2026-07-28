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
include(GNUInstallDirs)

option(ASGE_INSTALL "Generate install rules for the ASGE library" ${PROJECT_IS_TOP_LEVEL})

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
