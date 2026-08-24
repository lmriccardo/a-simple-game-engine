add_executable(ASGE_AssetsTests
    AssetPoolTests.cpp
    AssetManagerTests.cpp
)

target_link_libraries(ASGE_AssetsTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

target_compile_definitions(ASGE_AssetsTests
    PRIVATE ASGE_TEST_FONTS_DIR="${CMAKE_SOURCE_DIR}/tests/support/fonts")

target_compile_options(ASGE_AssetsTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_AssetsTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_AssetsTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_AssetsTests)
