add_executable(scoped_profiler
    main.cpp
)
target_link_libraries(scoped_profiler PRIVATE ASGE)
set_target_properties(scoped_profiler
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(scoped_profiler)
