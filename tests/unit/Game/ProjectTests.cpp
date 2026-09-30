#include <ASGE/Game/Project/Project.hpp>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>

#include <gtest/gtest.h>

#include <filesystem>

namespace
{

using asge::errors::ConfError;
using asge::game::project::LoadProjectFile;

class ProjectTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Root;
    std::filesystem::path m_File;

    void SetUp() override
    {
        m_Root = std::filesystem::temp_directory_path()
            / ("asge_project_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::filesystem::create_directories(m_Root);
        m_File = m_Root / "demo.asgeproject";
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Root, ec);
    }

    void Write(std::string const& inMountDir, std::vector<std::string> const& inScenes, std::string const& inMain = {})
    {
        asge::config::toml::TOMLBuilder builder;
        auto mount = builder.ArrayTable("Mount");
        mount.Set("Name", std::string("assets"));
        mount.Set("RealDirectory", inMountDir);
        builder.SetArray("Scenes", inScenes);
        if (!inMain.empty()) builder.Set("MainScene", inMain);
        ASSERT_TRUE(builder.SaveToFile(m_File).IsOk());
    }
};

// ─── LoadProjectFile ─────────────────────────────────────────────────────────

TEST_F(ProjectTest, LoadProjectFile_AbsolutePaths_AreKeptAsWritten)
{
    auto const scene = (m_Root / "a.asgescene").string();
    Write(m_Root.string(), { scene });

    auto result = LoadProjectFile(m_File);

    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value().m_Mounts.size(), 1u);
    EXPECT_EQ(result.Value().m_Mounts[0].m_Name, "assets");
    EXPECT_EQ(result.Value().m_Mounts[0].m_RealDirectory, m_Root);
    ASSERT_EQ(result.Value().m_Scenes.size(), 1u);
    EXPECT_EQ(result.Value().m_Scenes[0], std::filesystem::path(scene));
}

TEST_F(ProjectTest, LoadProjectFile_RelativePaths_ResolveAgainstTheProjectFolder)
{
    Write("assets", { "scenes/a.asgescene", "scenes/b.asgescene" });

    auto result = LoadProjectFile(m_File);

    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().m_Mounts[0].m_RealDirectory, (m_Root / "assets").lexically_normal());
    ASSERT_EQ(result.Value().m_Scenes.size(), 2u);
    EXPECT_EQ(result.Value().m_Scenes[0], (m_Root / "scenes/a.asgescene").lexically_normal());
    EXPECT_EQ(result.Value().m_Scenes[1], (m_Root / "scenes/b.asgescene").lexically_normal());
}

TEST_F(ProjectTest, LoadProjectFile_NoScenesOrMounts_YieldsEmptyLists)
{
    asge::config::toml::TOMLBuilder builder;
    builder.SetArray("Scenes", std::vector<std::string>{});
    ASSERT_TRUE(builder.SaveToFile(m_File).IsOk());

    auto result = LoadProjectFile(m_File);

    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().m_Mounts.empty());
    EXPECT_TRUE(result.Value().m_Scenes.empty());
}

TEST_F(ProjectTest, LoadProjectFile_MainScene_ResolvesAgainstTheProjectFolder)
{
    Write("assets", { "a.asgescene", "b.asgescene" }, "b.asgescene");

    auto result = LoadProjectFile(m_File);

    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().m_MainScene, (m_Root / "b.asgescene").lexically_normal());
}

TEST_F(ProjectTest, LoadProjectFile_ViewTable_GivesTheGameWindowSize)
{
    asge::config::toml::TOMLBuilder builder;
    builder.SetArray("Scenes", std::vector<std::string>{ "a.asgescene" });
    auto view = builder.Table("View");
    view.Set("TargetWidth", 1280);
    view.Set("TargetHeight", 720);
    ASSERT_TRUE(builder.SaveToFile(m_File).IsOk());

    auto result = LoadProjectFile(m_File);

    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().m_TargetWidth, 1280);
    EXPECT_EQ(result.Value().m_TargetHeight, 720);
}

TEST_F(ProjectTest, LoadProjectFile_NoViewTable_LeavesTheWindowSizeUnset)
{
    Write("assets", { "a.asgescene" });

    auto result = LoadProjectFile(m_File);

    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value().m_TargetWidth, 0);
    EXPECT_EQ(result.Value().m_TargetHeight, 0);
}

TEST_F(ProjectTest, LoadProjectFile_NoMainScene_LeavesItEmpty)
{
    Write("assets", { "a.asgescene" });

    auto result = LoadProjectFile(m_File);

    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value().m_MainScene.empty());
}

TEST_F(ProjectTest, LoadProjectFile_MissingFile_ReturnsError)
{
    EXPECT_FALSE(LoadProjectFile(m_Root / "nope.asgeproject").IsOk());
}

}
