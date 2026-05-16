add_executable(ASGE_AllocatorTest
    LinearAllocatorTests.cpp
    ArenaAllocatorTests.cpp
)

target_link_libraries(ASGE_AllocatorTest
    PRIVATE
        ASGE
        GTest::gtest_main
)

target_compile_options(ASGE_AllocatorTest
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_AllocatorTest POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_AllocatorTest>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_AllocatorTest)
