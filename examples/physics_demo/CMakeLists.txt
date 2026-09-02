add_executable(physics_demo
    Game.cpp
    main.cpp
)
target_link_libraries(physics_demo PRIVATE ASGE)
set_target_properties(physics_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(physics_demo)
