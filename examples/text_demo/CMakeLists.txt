add_executable(text_demo
    Game.cpp
    main.cpp
)
target_link_libraries(text_demo PRIVATE ASGE)
target_compile_definitions(text_demo
    PRIVATE ASGE_TEXT_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(text_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(text_demo)
