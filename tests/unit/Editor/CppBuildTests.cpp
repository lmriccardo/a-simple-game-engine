#include "CppBuild.hpp"
#include "CppProject.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{

namespace fs = std::filesystem;

class CppBuildTest : public ::testing::Test
{
protected:
    fs::path m_Root;
    fs::path m_ProjectFile;

    void SetUp() override
    {
        m_Root = fs::temp_directory_path()
            / ("asge_cpp_build_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        fs::create_directories(m_Root);
        m_ProjectFile = m_Root / "my game.asgeproject";
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(m_Root, ec);
    }

    void Touch(fs::path const& inPath)
    {
        fs::create_directories(inPath.parent_path());
        std::ofstream(inPath) << "x";
    }
};

// ─── Names and paths ─────────────────────────────────────────────────────────

TEST_F(CppBuildTest, CppTargetName_IsTheProjectStemAsAnIdentifier)
{
    EXPECT_EQ(CppTargetName(m_ProjectFile), "my_game");
}

TEST_F(CppBuildTest, CppBuildDir_IsCodeBuildInsideTheProjectFolder)
{
    EXPECT_EQ(CppBuildDir(m_ProjectFile), m_Root / "code" / "build");
}

TEST_F(CppBuildTest, FindCppExecutable_NothingBuilt_ReturnsNullopt)
{
    EXPECT_FALSE(FindCppExecutable(m_ProjectFile).has_value());
}

TEST_F(CppBuildTest, FindCppExecutable_PrefersTheDebugFolder)
{
    auto const build = CppBuildDir(m_ProjectFile);
    Touch(build / "Debug" / "my_game.exe");
    Touch(build / "my_game.exe");

    EXPECT_EQ(FindCppExecutable(m_ProjectFile), build / "Debug" / "my_game.exe");
}

TEST_F(CppBuildTest, FindCppExecutable_FindsASingleConfigBuild)
{
    auto const build = CppBuildDir(m_ProjectFile);
    Touch(build / "my_game");

    EXPECT_EQ(FindCppExecutable(m_ProjectFile), build / "my_game");
}

// ─── RunProcess ──────────────────────────────────────────────────────────────

TEST_F(CppBuildTest, RunProcess_CapturesOutputLinesAndReturnsTheExitCode)
{
    std::vector<std::string> lines;

    auto const exitCode = RunProcess({ "cmake", "--version" }, [&](std::string const& inLine) { lines.push_back(inLine); });

    EXPECT_EQ(exitCode, 0);
    ASSERT_FALSE(lines.empty());
    EXPECT_NE(lines.front().find("cmake version"), std::string::npos);
}

TEST_F(CppBuildTest, RunProcess_NonZeroExit_IsReturned)
{
    auto const exitCode = RunProcess({ "cmake", "-E", "false" }, [](std::string const&) {});

    EXPECT_NE(exitCode, 0);
}

TEST_F(CppBuildTest, RunProcess_ProgramThatDoesNotExist_ReturnsMinusOneAndReportsIt)
{
    std::vector<std::string> lines;

    auto const exitCode = RunProcess({ "asge-no-such-program" }, [&](std::string const& inLine) { lines.push_back(inLine); });

    EXPECT_EQ(exitCode, -1);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines.front().find("could not start"), std::string::npos);
}

// ─── CppBuild ────────────────────────────────────────────────────────────────

TEST_F(CppBuildTest, Run_WithNothingBuilt_ReturnsToIdle)
{
    CppBuild build;

    build.Run(m_ProjectFile);
    for (int wait = 0; wait < 200 && build.GetState() != CppBuild::State::Idle; ++wait)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    EXPECT_EQ(build.GetState(), CppBuild::State::Idle);
}

TEST_F(CppBuildTest, Compile_WithoutACppProject_FailsAndReturnsToIdle)
{
    CppBuild build;

    build.Compile(m_ProjectFile); // no code/ folder: cmake cannot configure it
    for (int wait = 0; wait < 400 && build.GetState() != CppBuild::State::Idle; ++wait)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    EXPECT_EQ(build.GetState(), CppBuild::State::Idle);
}

}
