add_executable(image_info
    main.cpp
)
target_link_libraries(image_info PRIVATE ASGE)
target_compile_definitions(image_info
    PRIVATE ASGE_IMAGE_INFO_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
set_target_properties(image_info
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(image_info)
