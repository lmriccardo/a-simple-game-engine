add_executable(path_following_demo
    Game.cpp
    main.cpp
)
target_link_libraries(path_following_demo PRIVATE ASGE)
target_compile_definitions(path_following_demo
    PRIVATE ASGE_PATH_FOLLOWING_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(path_following_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(path_following_demo)
