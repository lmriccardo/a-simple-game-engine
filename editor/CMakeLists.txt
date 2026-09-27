add_executable(asge-editor main.cpp Inspector.cpp ViewportOverlay.cpp ConsolePanel.cpp FileDialog.cpp AssetBrowser.cpp AssetInspector.cpp VfsPanel.cpp ProjectManager.cpp)

target_link_libraries(asge-editor PRIVATE ASGE imgui::imgui)

target_compile_definitions(asge-editor PRIVATE
    ASGE_EDITOR_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")

target_compile_options(asge-editor
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

set_target_properties(asge-editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

asge_copy_sdl3_runtime(asge-editor)

# .exe file icon (Explorer, taskbar, Alt-Tab) -- Windows-only, resource-compiler
# concept with no cross-platform equivalent. The in-app window icon (title bar,
# taskbar while running) is set separately at runtime via SDL_SetWindowIcon
# from assets/icon.png (see main.cpp), since that's cross-platform and doesn't
# need a resource compiler.
if(WIN32)
    set(ASGE_EDITOR_ICON_ICO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
    configure_file(resources/asge-editor.rc.in "${CMAKE_CURRENT_BINARY_DIR}/asge-editor.rc" @ONLY)
    target_sources(asge-editor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/asge-editor.rc")
endif()
