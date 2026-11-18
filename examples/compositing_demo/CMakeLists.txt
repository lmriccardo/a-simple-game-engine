add_executable(compositing_demo
    Game.cpp
    main.cpp
)
target_link_libraries(compositing_demo PRIVATE ASGE)
set_target_properties(compositing_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(compositing_demo)
