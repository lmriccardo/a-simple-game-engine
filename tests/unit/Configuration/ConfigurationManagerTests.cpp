#include <ASGE/Core/Configuration/ConfigurationManager.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

using namespace asge::config;
using asge::errors::ConfError;

class ConfigurationManagerTest : public ::testing::Test
{
protected:
    std::filesystem::path m_TomlPath;

    void SetUp() override
    {
        auto const uniqueName = "asge_conf_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".toml";
        m_TomlPath = std::filesystem::temp_directory_path() / uniqueName;
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(m_TomlPath, ec);
    }

    void WriteToml(std::string const& inContent) const
    {
        std::ofstream file(m_TomlPath, std::ios::trunc);
        file << inContent;
    }
};

// ─── Load — error cases ───────────────────────────────────────────────────────

TEST_F(ConfigurationManagerTest, Load_NonExistentPathReturnsError)
{
    ConfigurationManager cm;
    auto result = cm.Load(m_TomlPath); // never written — does not exist on disk
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::InvalidInputPath));
}

TEST_F(ConfigurationManagerTest, Load_DirectoryPathReturnsError)
{
    ConfigurationManager cm;
    auto result = cm.Load(std::filesystem::temp_directory_path());
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::InvalidInputPath));
}

TEST_F(ConfigurationManagerTest, Load_WrongExtensionReturnsError)
{
    auto wrongPath = m_TomlPath;
    wrongPath.replace_extension(".txt");
    {
        std::ofstream file(wrongPath, std::ios::trunc);
        file << "key = 1\n";
    }

    ConfigurationManager cm;
    auto result = cm.Load(wrongPath);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::InvalidInputPath));

    std::error_code ec;
    std::filesystem::remove(wrongPath, ec);
}

// ─── Load — success + Get integration ─────────────────────────────────────────

TEST_F(ConfigurationManagerTest, Load_ValidTomlThenGetReturnsValues)
{
    WriteToml("count = 42\nname = \"Alice\"\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    auto count = cm.Get<int>("count");
    ASSERT_TRUE(count.IsOk());
    EXPECT_EQ(count.Value(), 42);

    auto name = cm.Get<std::string>("name");
    ASSERT_TRUE(name.IsOk());
    EXPECT_EQ(name.Value(), "Alice");
}

TEST_F(ConfigurationManagerTest, Load_SamePathTwiceReturnsOk)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());
}

TEST_F(ConfigurationManagerTest, Get_VectorTypeReturnsElements)
{
    WriteToml("nums = [1, 2, 3]\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    auto result = cm.Get<std::vector<int>>("nums");
    ASSERT_TRUE(result.IsOk());
    auto const& vec = result.Value();
    ASSERT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
}

// ─── Hot reload flag ──────────────────────────────────────────────────────────

TEST_F(ConfigurationManagerTest, Load_DoesNotEnableHotReloadByDefault)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());
    EXPECT_FALSE(cm.IsHotReloadEnabled());
}

TEST_F(ConfigurationManagerTest, SetHotReload_AfterLoadEnablesFlag)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    ASSERT_TRUE(cm.SetHotReloadEnabled(true).IsOk());
    EXPECT_TRUE(cm.IsHotReloadEnabled());
}

TEST_F(ConfigurationManagerTest, SetHotReload_BeforeLoadIsPickedUpByLoad)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.SetHotReloadEnabled(true).IsOk());
    EXPECT_TRUE(cm.IsHotReloadEnabled());

    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());
    EXPECT_TRUE(cm.IsHotReloadEnabled());
}

TEST_F(ConfigurationManagerTest, SetHotReload_FalseDisablesFlag)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());
    ASSERT_TRUE(cm.SetHotReloadEnabled(true).IsOk());
    ASSERT_TRUE(cm.SetHotReloadEnabled(false).IsOk());

    EXPECT_FALSE(cm.IsHotReloadEnabled());
}

// ─── Get — error cases ─────────────────────────────────────────────────────────

TEST_F(ConfigurationManagerTest, Get_BeforeLoadReturnsError)
{
    ConfigurationManager cm;
    auto result = cm.Get<int>("count");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::ConfigurationNotLoaded));
}

TEST_F(ConfigurationManagerTest, Get_MissingKeyReturnsError)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    auto result = cm.Get<int>("missing");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoAttribute));
}

TEST_F(ConfigurationManagerTest, Get_WrongTypeReturnsError)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    auto result = cm.Get<std::string>("count");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlTypeMismatch));
}

}
