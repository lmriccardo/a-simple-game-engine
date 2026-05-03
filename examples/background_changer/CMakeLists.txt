add_executable(background_changer
    Game.cpp
    Component.cpp
    main.cpp
)
target_link_libraries(background_changer PRIVATE ASGE)
set_target_properties(background_changer
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(background_changer)