add_executable(ecs_demo
    Game.cpp
    main.cpp
)
target_link_libraries(ecs_demo PRIVATE ASGE)
target_compile_definitions(ecs_demo
    PRIVATE ASGE_ECS_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(ecs_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(ecs_demo)
