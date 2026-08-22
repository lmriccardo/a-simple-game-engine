add_executable(input_demo
    Game.cpp
    main.cpp
)
target_link_libraries(input_demo PRIVATE ASGE)
set_target_properties(input_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(input_demo)
