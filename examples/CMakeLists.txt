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

    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3>
                $<TARGET_FILE_DIR:${target_name}>
        )
    endif()
endmacro()

add_subdirectory(background_changer)
add_subdirectory(moving_box)