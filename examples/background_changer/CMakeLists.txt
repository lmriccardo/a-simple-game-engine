add_executable(background_changer
    src/Game.cpp
    src/Component.cpp
    main.cpp
)
target_link_libraries(background_changer PRIVATE ASGE)
set_target_properties(background_changer
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

target_compile_options(background_changer
    PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
)

if(WIN32)
    add_custom_command(TARGET background_changer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:background_changer>
    )
endif()