add_executable(camera_follow_demo
    Game.cpp
    main.cpp
)
target_link_libraries(camera_follow_demo PRIVATE ASGE)
target_compile_definitions(camera_follow_demo
    PRIVATE ASGE_CAMERA_FOLLOW_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(camera_follow_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(camera_follow_demo)
