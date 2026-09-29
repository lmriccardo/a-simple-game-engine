add_executable(ui_demo
    Game.cpp
    main.cpp
)
target_link_libraries(ui_demo PRIVATE ASGE)
target_compile_definitions(ui_demo
    PRIVATE ASGE_UI_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(ui_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(ui_demo)
