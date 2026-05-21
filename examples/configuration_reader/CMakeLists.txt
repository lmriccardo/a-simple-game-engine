add_executable(configuration_reader
    main.cpp
)
target_link_libraries(configuration_reader PRIVATE ASGE)
set_target_properties(configuration_reader
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(configuration_reader)