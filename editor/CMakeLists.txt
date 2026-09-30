add_executable(asge-editor main.cpp Inspector.cpp ViewportOverlay.cpp ConsolePanel.cpp FileDialog.cpp AssetBrowser.cpp AssetInspector.cpp VfsPanel.cpp ProjectManager.cpp CppProject.cpp)

target_link_libraries(asge-editor PRIVATE ASGE imgui::imgui)

target_compile_definitions(asge-editor PRIVATE
    ASGE_EDITOR_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    ASGE_EDITOR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

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

# ── Packaging (CPack) ─────────────────────────────────────────────────────
# Everything below only matters for `cpack`'s installer output, gated behind
# the same ASGE_INSTALL switch the SDK's own install() rules already use --
# a dev build (ASGE_INSTALL=OFF, the common case) skips all of it.
if(ASGE_INSTALL)
    if(APPLE)
        # A macOS .app bundle instead of a bare executable, so Finder/
        # LaunchServices has somewhere to hang the file-type/document
        # associations below. MACOSX_PACKAGE_LOCATION copies each resource
        # into Contents/Resources at build time (not just on `cmake --install`),
        # so cpack's DragNDrop generator packages a bundle that's already
        # correct straight out of the build tree.
        set_target_properties(asge-editor PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
        configure_file(
            packaging/macos/Info.plist.in
            "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)

        set(ASGE_EDITOR_BUNDLE_RESOURCES
            assets/icon.icns
            assets/asge-project-icon.icns
            assets/asge-scene-icon.icns
        )
        set_source_files_properties(${ASGE_EDITOR_BUNDLE_RESOURCES} PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources)
        target_sources(asge-editor PRIVATE ${ASGE_EDITOR_BUNDLE_RESOURCES})
    elseif(WIN32)
        install(TARGETS asge-editor RUNTIME DESTINATION .)
        if(TARGET SDL3::SDL3-shared)
            install(FILES "$<TARGET_FILE:SDL3::SDL3-shared>" DESTINATION .)
        endif()
        install(DIRECTORY assets/ DESTINATION assets)
    else()
        # Linux: a normal FHS-ish layout under the install prefix, plus a
        # .desktop launcher and the shared-mime-info/icon-theme files that
        # give .asgeproject/.asgescene their icons in a file manager --
        # NSIS's registry keys and the macOS Info.plist above are the same
        # idea, just each platform's own mechanism for it.
        install(TARGETS asge-editor RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        install(DIRECTORY assets/ DESTINATION "${CMAKE_INSTALL_DATADIR}/asge-editor/assets")
        install(FILES packaging/linux/asge-editor.desktop
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
        install(FILES packaging/linux/asge-mime.xml
            DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages"
            RENAME asge-editor.xml)
        install(FILES assets/asge-project-icon.png
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/mimetypes"
            RENAME application-x-asge-project.png)
        install(FILES assets/asge-scene-icon.png
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/mimetypes"
            RENAME application-x-asge-scene.png)
        install(FILES assets/icon.png
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps"
            RENAME asge-editor.png)
    endif()
endif()
