add_executable(audio_demo
    Game.cpp
    main.cpp
)
target_link_libraries(audio_demo PRIVATE ASGE)
target_compile_definitions(audio_demo
    PRIVATE ASGE_AUDIO_DEMO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(audio_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(audio_demo)
