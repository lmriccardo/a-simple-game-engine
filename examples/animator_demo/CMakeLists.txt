add_executable(animator_demo
    Game.cpp
    main.cpp
)
target_link_libraries(animator_demo PRIVATE ASGE)
target_compile_definitions(animator_demo
    PRIVATE ASGE_ANIMATOR_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(animator_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(animator_demo)
