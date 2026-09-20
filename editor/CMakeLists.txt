add_executable(asge-editor main.cpp)

target_link_libraries(asge-editor PRIVATE ASGE imgui::imgui)

# Phase 1's hardcoded test scene reuses scene_demo's fixture rather than
# duplicating it -- see main.cpp's own comment.
target_compile_definitions(asge-editor PRIVATE
    ASGE_EDITOR_TEST_SCENE_DIR="${CMAKE_SOURCE_DIR}/examples/scene_demo/assets")

target_compile_options(asge-editor
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

set_target_properties(asge-editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

asge_copy_sdl3_runtime(asge-editor)
