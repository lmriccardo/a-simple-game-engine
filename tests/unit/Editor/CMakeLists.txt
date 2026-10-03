add_executable(ASGE_EditorTests
    CppProjectTests.cpp
    CppBuildTests.cpp
    ToolchainTests.cpp
    BuildOutputTests.cpp
    EntityGroupsTests.cpp
    AssetTreeTests.cpp
    ${CMAKE_SOURCE_DIR}/editor/CppProject.cpp
    ${CMAKE_SOURCE_DIR}/editor/CppBuild.cpp
    ${CMAKE_SOURCE_DIR}/editor/Toolchain.cpp
    ${CMAKE_SOURCE_DIR}/editor/BuildOutput.cpp
    ${CMAKE_SOURCE_DIR}/editor/EntityGroups.cpp
    ${CMAKE_SOURCE_DIR}/editor/AssetTree.cpp
)

target_include_directories(ASGE_EditorTests PRIVATE "${CMAKE_SOURCE_DIR}/editor")

target_link_libraries(ASGE_EditorTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

target_compile_options(ASGE_EditorTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_EditorTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_EditorTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_EditorTests)
