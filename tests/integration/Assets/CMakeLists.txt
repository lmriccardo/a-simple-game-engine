add_executable(ASGE_AssetsIntegrationTests
    AssetManagerIntegrationTests.cpp
)

target_link_libraries(ASGE_AssetsIntegrationTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

# SDLHeadlessFixture.hpp is shared with tests/unit/Video and tests/integration/Video
target_include_directories(ASGE_AssetsIntegrationTests PRIVATE "${CMAKE_SOURCE_DIR}/tests/support")

# Ahem.ttf (also used by tests/unit/Graphics/Video and tests/integration/Video)
# -- see tests/support/fonts/NOTICE.md
target_compile_definitions(ASGE_AssetsIntegrationTests
    PRIVATE ASGE_TEST_FONTS_DIR="${CMAKE_SOURCE_DIR}/tests/support/fonts")

target_compile_options(ASGE_AssetsIntegrationTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_AssetsIntegrationTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_AssetsIntegrationTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_AssetsIntegrationTests)
