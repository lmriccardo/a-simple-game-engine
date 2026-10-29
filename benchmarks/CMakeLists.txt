macro(setup_benchmark target_name)
    target_link_libraries(${target_name} PRIVATE ASGE)

    target_compile_options(${target_name}
        PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
                -Wall -Wextra -Wpedantic
            >
            $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    )

    asge_copy_sdl3_runtime(${target_name})
endmacro()

add_subdirectory(quadtree)
