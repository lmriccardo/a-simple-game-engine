add_executable(file_watching
    main.cpp
)
target_link_libraries(file_watching PRIVATE ASGE)
set_target_properties(file_watching
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(file_watching)