add_executable(ASGE_VideoTests
    VideoErrorsTests.cpp
    GraphicsInterfaceTests.cpp
    SDLTextureTests.cpp
    SDLRendererTextureTests.cpp
)

target_link_libraries(ASGE_VideoTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

# SDLHeadlessFixture.hpp is shared with tests/integration/Video
target_include_directories(ASGE_VideoTests PRIVATE "${CMAKE_SOURCE_DIR}/tests/support")

# Ahem.ttf (also used by tests/integration/Video) -- see tests/support/fonts/NOTICE.md
target_compile_definitions(ASGE_VideoTests
    PRIVATE ASGE_TEST_FONTS_DIR="${CMAKE_SOURCE_DIR}/tests/support/fonts")

target_compile_options(ASGE_VideoTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_VideoTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_VideoTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_VideoTests)
