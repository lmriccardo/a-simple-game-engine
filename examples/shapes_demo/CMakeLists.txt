add_executable(shapes_demo
    Game.cpp
    main.cpp
)
target_link_libraries(shapes_demo PRIVATE ASGE)
set_target_properties(shapes_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(shapes_demo)
