add_executable(rotation_demo
    Game.cpp
    main.cpp
)
target_link_libraries(rotation_demo PRIVATE ASGE)
target_compile_definitions(rotation_demo
    PRIVATE ASGE_ROTATION_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(rotation_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(rotation_demo)
