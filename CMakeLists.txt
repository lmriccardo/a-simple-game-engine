cmake_minimum_required(VERSION 3.31.0)
project(asge VERSION 0.8.6 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ASGE_EDITOR_VERSION 1.3)
set(PACKAGE_VERSION "${PROJECT_VERSION}e${ASGE_EDITOR_VERSION}")

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

add_library(stb_vorbis STATIC ${CMAKE_CURRENT_SOURCE_DIR}/third-party/stb/stb_vorbis.c)
add_library(stb::stb_vorbis ALIAS stb_vorbis)
target_include_directories(stb_vorbis PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party/stb>")

add_subdirectory(src/ASGE)

option(ASGE_BUILD_EXAMPLES "Build the examples folder" ON)
if(ASGE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

option(ASGE_BUILD_EDITOR "Build the asge-editor tool" ON)
if(ASGE_BUILD_EDITOR)
    # Dear ImGui, vendored via scripts/install-imgui.* into third-party/imgui --
    # compiled from source (no prebuilt lib/find_package, unlike SDL) since
    # that's how ImGui is meant to be consumed. Only the core widgets/backend
    # needed by asge-editor are built; multi-viewport/docking are opt-in flags
    # ImGui itself gates at runtime, not separate source files, so nothing extra
    # is pulled in by not enabling them.
    add_library(imgui STATIC
        third-party/imgui/imgui.cpp
        third-party/imgui/imgui_draw.cpp
        third-party/imgui/imgui_tables.cpp
        third-party/imgui/imgui_widgets.cpp
        third-party/imgui/imgui_demo.cpp
        third-party/imgui/misc/cpp/imgui_stdlib.cpp
        third-party/imgui/backends/imgui_impl_sdl3.cpp
        third-party/imgui/backends/imgui_impl_sdlrenderer3.cpp
    )
    add_library(imgui::imgui ALIAS imgui)
    target_include_directories(imgui PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/backends>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui/misc/cpp>")
    target_link_libraries(imgui PUBLIC SDL3::SDL3)

    add_subdirectory(editor)
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

# ── CPack: end-user installers for asge-editor ──────────────────────────────
# Wraps whatever install() rules are active (editor/CMakeLists.txt's, mainly)
# into a real platform installer: an NSIS .exe on Windows, a .deb on Linux,
# a drag-and-drop .dmg on macOS. `cpack` is a separate step after a normal
# build+install, not part of the default build -- run `cpack` (or `cpack -G
# NSIS`/`DEB`/`DragNDrop` explicitly) from the build directory once
# ASGE_INSTALL/ASGE_BUILD_EDITOR are both ON.
if(ASGE_INSTALL AND ASGE_BUILD_EDITOR)
    set(CPACK_PACKAGE_NAME "ASGE Editor")
    set(CPACK_PACKAGE_VENDOR "ASGE")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Simple Game Engine -- Level Editor")
    set(CPACK_PACKAGE_VERSION "${PACKAGE_VERSION}")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/lmriccardo/a-simple-game-engine")
    if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
        set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
    endif()

    if(WIN32)
        set(CPACK_GENERATOR "NSIS")
        set(CPACK_PACKAGE_FILE_NAME "asge-editor-${PACKAGE_VERSION}-windows-x64")
        set(CPACK_PACKAGE_INSTALL_DIRECTORY "ASGE Editor")
        set(CPACK_NSIS_DISPLAY_NAME "ASGE Editor")
        set(CPACK_NSIS_PACKAGE_NAME "ASGE Editor")
        set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/editor/assets/icon.ico")
        set(CPACK_NSIS_INSTALLED_ICON_NAME "asge-editor.exe")

        # Registers .asgeproject/.asgescene's Explorer icon under HKCU (no
        # admin rights needed) -- the installer-time equivalent of the .reg
        # files the editor itself ships (see docs/level_editor_roadmap), but
        # pointing at wherever THIS install actually landed ($INSTDIR)
        # instead of a hardcoded dev-machine path, and cleaned up on
        # uninstall.
        #
        # Backslashes are doubled, not single, even though bracket syntax
        # ([[...]]) itself does no escape processing: CPack re-serializes
        # this variable into a normal double-quoted set() inside the
        # CPackConfig.cmake it generates, and that file is then re-parsed as
        # CMake script by `cpack` itself -- a single "\C"/"\a" there is an
        # invalid escape sequence. Doubling here is what survives that one
        # extra round-trip and lands as a single backslash in the final .nsi.
        #
        # Single-quoted NSIS string literals, not double-quoted, for the
        # same reason: that same re-serialization wraps this whole block in
        # ONE outer double-quoted string with no escaping of embedded quotes
        # -- a double-quoted literal in here would prematurely close it. NSIS
        # itself accepts either quote style, so single-quoting sidesteps the
        # collision entirely rather than fighting it.
        set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS [[
  WriteRegStr HKCU 'Software\\Classes\\.asgeproject' '' 'ASGE.Project'
  WriteRegStr HKCU 'Software\\Classes\\ASGE.Project' '' 'ASGE Project'
  WriteRegStr HKCU 'Software\\Classes\\ASGE.Project\\DefaultIcon' '' '$INSTDIR\\assets\\asge-project-icon.ico,0'
  WriteRegStr HKCU 'Software\\Classes\\.asgescene' '' 'ASGE.Scene'
  WriteRegStr HKCU 'Software\\Classes\\ASGE.Scene' '' 'ASGE Scene'
  WriteRegStr HKCU 'Software\\Classes\\ASGE.Scene\\DefaultIcon' '' '$INSTDIR\\assets\\asge-scene-icon.ico,0'
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
]])
        set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS [[
  DeleteRegKey HKCU 'Software\\Classes\\.asgeproject'
  DeleteRegKey HKCU 'Software\\Classes\\ASGE.Project'
  DeleteRegKey HKCU 'Software\\Classes\\.asgescene'
  DeleteRegKey HKCU 'Software\\Classes\\ASGE.Scene'
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, i 0, i 0)'
]])
    elseif(APPLE)
        set(CPACK_GENERATOR "DragNDrop")
        set(CPACK_PACKAGE_FILE_NAME "asge-editor-${PACKAGE_VERSION}-macos-arm64")
        set(CPACK_DMG_VOLUME_NAME "ASGE Editor")
    else()
        set(CPACK_GENERATOR "DEB")
        set(CPACK_PACKAGE_FILE_NAME "asge-editor-${PACKAGE_VERSION}-linux-x64")
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "lmriccardo <noreply@github.com>")
        set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
        set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
            "${CMAKE_SOURCE_DIR}/editor/packaging/linux/postinst"
            "${CMAKE_SOURCE_DIR}/editor/packaging/linux/postrm")
    endif()

    include(CPack)
endif()
