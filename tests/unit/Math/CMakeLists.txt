add_executable(ASGE_MathTests
    VectorNTests.cpp
    Vector2Tests.cpp
    CircleTests.cpp
)

target_link_libraries(ASGE_MathTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

target_compile_options(ASGE_MathTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_MathTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_MathTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_MathTests)
