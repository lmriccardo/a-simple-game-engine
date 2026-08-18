add_executable(texture_demo
    Game.cpp
    main.cpp
)
target_link_libraries(texture_demo PRIVATE ASGE)
target_compile_definitions(texture_demo
    PRIVATE ASGE_TEXTURE_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(texture_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(texture_demo)
