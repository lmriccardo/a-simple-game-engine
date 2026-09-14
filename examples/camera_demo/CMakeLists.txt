add_executable(camera_demo
    Game.cpp
    main.cpp
)
target_link_libraries(camera_demo PRIVATE ASGE)
set_target_properties(camera_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(camera_demo)
