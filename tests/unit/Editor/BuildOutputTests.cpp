#include "BuildOutput.hpp"
#include "CppBuild.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <thread>

namespace
{

class BuildOutputTest : public ::testing::Test
{
protected:
    void SetUp() override { ClearBuildOutput(); }
    void TearDown() override { ClearBuildOutput(); }
};

TEST_F(BuildOutputTest, Append_KeepsLinesInOrderWithTheirLevels)
{
    AppendBuildOutput(BuildLevel::Info, "one");
    AppendBuildOutput(BuildLevel::Error, "two");

    auto const lines = SnapshotBuildOutput();

    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].m_Text, "one");
    EXPECT_EQ(lines[0].m_Level, BuildLevel::Info);
    EXPECT_EQ(lines[1].m_Text, "two");
    EXPECT_EQ(lines[1].m_Level, BuildLevel::Error);
}

TEST_F(BuildOutputTest, Clear_RemovesEverything)
{
    AppendBuildOutput(BuildLevel::Info, "one");

    ClearBuildOutput();

    EXPECT_TRUE(SnapshotBuildOutput().empty());
}

TEST_F(BuildOutputTest, PastTheCap_TheOldestLinesAreDropped)
{
    for (int i = 0; i < 5100; ++i) AppendBuildOutput(BuildLevel::Info, std::to_string(i));

    auto const lines = SnapshotBuildOutput();

    ASSERT_EQ(lines.size(), 5000u);
    EXPECT_EQ(lines.front().m_Text, "100");
    EXPECT_EQ(lines.back().m_Text, "5099");
}

TEST_F(BuildOutputTest, ConcurrentAppends_AreAllKept)
{
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([] { for (int i = 0; i < 100; ++i) AppendBuildOutput(BuildLevel::Info, "x"); });
    }
    for (auto& thread : threads) thread.join();

    EXPECT_EQ(SnapshotBuildOutput().size(), 400u);
}

TEST_F(BuildOutputTest, CppBuildRun_WithNothingBuilt_ReportsInTheBuildOutput)
{
    auto const root = std::filesystem::temp_directory_path() / "asge_build_output_test_nothing_built";
    CppBuild build;

    build.Run(root / "game.asgeproject");
    for (int wait = 0; wait < 200 && build.GetState() != CppBuild::State::Idle; ++wait)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    auto const lines = SnapshotBuildOutput();
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines.back().m_Level, BuildLevel::Error);
    EXPECT_NE(lines.back().m_Text.find("compile first"), std::string::npos);
}

}
