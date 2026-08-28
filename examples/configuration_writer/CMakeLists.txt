add_executable(configuration_writer
    main.cpp
)
target_link_libraries(configuration_writer PRIVATE ASGE)
set_target_properties(configuration_writer
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(configuration_writer)
