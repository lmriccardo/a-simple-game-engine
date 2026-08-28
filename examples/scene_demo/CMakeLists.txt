add_executable(scene_demo
    Game.cpp
    main.cpp
)
target_link_libraries(scene_demo PRIVATE ASGE)
target_compile_definitions(scene_demo
    PRIVATE ASGE_SCENE_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(scene_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(scene_demo)
