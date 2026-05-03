add_executable(moving_box
    MovingBoxGame.cpp
    MovingBox.cpp
    main.cpp
)
target_link_libraries(moving_box PRIVATE ASGE)
set_target_properties(moving_box
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(moving_box)