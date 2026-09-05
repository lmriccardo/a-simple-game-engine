add_executable(ASGE_ApplicationTests
    ApplicationTests.cpp
)

target_link_libraries(ASGE_ApplicationTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

target_compile_options(ASGE_ApplicationTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_ApplicationTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_ApplicationTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_ApplicationTests)
