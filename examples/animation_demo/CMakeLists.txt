add_executable(animation_demo
    Game.cpp
    main.cpp
)
target_link_libraries(animation_demo PRIVATE ASGE)
target_compile_definitions(animation_demo
    PRIVATE ASGE_ANIMATION_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(animation_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(animation_demo)
