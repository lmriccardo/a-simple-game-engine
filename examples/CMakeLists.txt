macro(setup_example target_name)
    target_compile_options(${target_name}
        PRIVATE
            # Setup warnings for Cxx Linux compiler
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
                -Wall -Wextra -Wpedantic
            >
            # Setup warnings for MSVC compiler
            $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    )

    asge_copy_sdl3_runtime(${target_name})
endmacro()

add_subdirectory(background_changer)
add_subdirectory(moving_box)
add_subdirectory(shapes_demo)
add_subdirectory(texture_demo)
add_subdirectory(text_demo)
add_subdirectory(configuration_reader)
add_subdirectory(configuration_writer)
add_subdirectory(image_info)
add_subdirectory(file_watching)
add_subdirectory(scoped_profiler)
add_subdirectory(ecs_demo)
add_subdirectory(input_demo)
add_subdirectory(scene_demo)
add_subdirectory(physics_demo)