add_executable(xml_demo
    main.cpp
)
target_link_libraries(xml_demo PRIVATE ASGE)
target_compile_definitions(xml_demo
    PRIVATE XML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/test.xml")
set_target_properties(xml_demo
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

setup_example(xml_demo)
