add_executable(ASGE_VideoIntegrationTests
    TextureRenderingIntegrationTests.cpp
)

target_link_libraries(ASGE_VideoIntegrationTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

# SDLHeadlessFixture.hpp is shared with tests/unit/Video
target_include_directories(ASGE_VideoIntegrationTests PRIVATE "${CMAKE_SOURCE_DIR}/tests/support")

target_compile_options(ASGE_VideoIntegrationTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_VideoIntegrationTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_VideoIntegrationTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_VideoIntegrationTests)
