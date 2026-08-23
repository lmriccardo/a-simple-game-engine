add_executable(ASGE_FilesystemTests
    VirtualFileSystemTests.cpp
)

target_link_libraries(ASGE_FilesystemTests
    PRIVATE
        ASGE
        GTest::gtest_main
)

target_compile_options(ASGE_FilesystemTests
    PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
            -Wall -Wextra -Wpedantic
        >
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if(WIN32)
    add_custom_command(TARGET ASGE_FilesystemTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:ASGE_FilesystemTests>
    )
endif()

include(GoogleTest)
gtest_discover_tests(ASGE_FilesystemTests)
