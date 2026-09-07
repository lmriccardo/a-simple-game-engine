add_executable(ASGE_AudioTests
    AudioClipTests.cpp
)

target_link_libraries(ASGE_AudioTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

target_compile_definitions(ASGE_AudioTests
    PRIVATE ASGE_TEST_AUDIO_DIR="${CMAKE_SOURCE_DIR}/tests/support/audio")

target_compile_options(ASGE_AudioTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_AudioTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_AudioTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_AudioTests)
