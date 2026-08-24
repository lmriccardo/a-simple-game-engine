cmake_minimum_required(VERSION 3.31.0)
project(asge VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(GNUInstallDirs)

option(ASGE_INSTALL "Generate install rules for the ASGE library" ${PROJECT_IS_TOP_LEVEL})

# Search for SDL library. Deliberately CMAKE_CURRENT_SOURCE_DIR (== this
# file's own directory) rather than CMAKE_SOURCE_DIR (the outermost
# listfile's directory): when ASGE is pulled in via add_subdirectory() from
# another project, CMAKE_SOURCE_DIR points at that project's root, not this
# repo, so the vendored SDL3 config would never be found.
if(WIN32)
    set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL/cmake")
else()
    set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/SDL/lib/cmake/SDL3")
endif()

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)

# find_package(CONFIG) imports the real SDL3::SDL3-shared/-static targets
# (and the SDL3::SDL3 convenience name, an ALIAS of whichever one exists)
# only into this directory's scope by default, so a consumer's own
# top-level CMakeLists.txt -- the one directory add_subdirectory() can
# never expose targets upward to -- would otherwise never see them (e.g.
# to copy the runtime DLL next to its own executable, the same way
# examples/CMakeLists.txt does). Promote the real targets to global
# visibility so they resolve from any directory scope; ALIAS targets can't
# take IMPORTED_GLOBAL directly (and, being aliases created before this
# runs, wouldn't retroactively become global anyway), so consumers reach
# SDL3 through the asge_copy_sdl3_runtime() helper below rather than
# through SDL3::SDL3 itself.
if(TARGET SDL3::SDL3-shared)
    set_target_properties(SDL3::SDL3-shared PROPERTIES IMPORTED_GLOBAL TRUE)
endif()
if(TARGET SDL3::SDL3-static)
    set_target_properties(SDL3::SDL3-static PROPERTIES IMPORTED_GLOBAL TRUE)
endif()

# Copies the SDL3 runtime DLL next to <target>'s output on Windows, the way
# examples/CMakeLists.txt's setup_example() does for in-repo examples.
# Consumers linking ASGE from another project should call this instead of
# reaching for SDL3::SDL3 themselves -- see the IMPORTED_GLOBAL note above
# for why that target isn't reliably visible outside this directory.
function(asge_copy_sdl3_runtime target_name)
    if(WIN32 AND TARGET SDL3::SDL3-shared)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3-shared>"
                "$<TARGET_FILE_DIR:${target_name}>"
        )
    endif()
endfunction()

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
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party/stb>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

if(ASGE_INSTALL)
    foreach(stb_header IN LISTS ASGE_STB_HEADERS)
        install(
            FILES "${CMAKE_CURRENT_SOURCE_DIR}/third-party/stb/${stb_header}"
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
