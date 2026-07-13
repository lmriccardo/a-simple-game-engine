#include <ASGE/Core/Configuration/ConfigurationManager.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
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

    std::string ReadToml() const
    {
        std::ifstream file(m_TomlPath);
        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
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

// ─── Set ──────────────────────────────────────────────────────────────────────

TEST_F(ConfigurationManagerTest, Set_UpdatesLiveValue_GetReflectsChange)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());
    ASSERT_TRUE(cm.Set<int>("count", 42).IsOk());

    auto result = cm.Get<int>("count");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), 42);
}

TEST_F(ConfigurationManagerTest, Set_VectorUpdatesArray)
{
    WriteToml("nums = [1, 2, 3]\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());
    ASSERT_TRUE(cm.Set<std::vector<int>>("nums", {4, 5}).IsOk());

    auto result = cm.Get<std::vector<int>>("nums");
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value().size(), 2u);
    EXPECT_EQ(result.Value()[0], 4);
    EXPECT_EQ(result.Value()[1], 5);
}

TEST_F(ConfigurationManagerTest, Set_FailsWhenNotLoaded)
{
    ConfigurationManager cm;
    auto result = cm.Set<int>("count", 1);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::ConfigurationNotLoaded));
}

TEST_F(ConfigurationManagerTest, Set_FailsOnMissingKey)
{
    WriteToml("count = 1\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    auto result = cm.Set<int>("missing", 1);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::TomlNoAttribute));
}

// ─── Save ─────────────────────────────────────────────────────────────────────

TEST_F(ConfigurationManagerTest, Save_FailsWhenNotLoaded)
{
    ConfigurationManager cm;
    auto result = cm.Save();
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ConfError::ConfigurationNotLoaded));
}

TEST_F(ConfigurationManagerTest, Save_RoundTripPreservesFormattingAcrossSetAndReload)
{
    WriteToml(
        "count = 1\n"
        "pi = 3.14\n"
        "enabled = false\n"
        R"(basic_str = "hello")" "\n"
        "literal_str = 'raw\\nvalue'\n"
        "nums = [1, 2, 3]\n"
    );

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    ASSERT_TRUE(cm.Set<int>("count", 2).IsOk());
    ASSERT_TRUE(cm.Set<double>("pi", 4.0).IsOk());       // whole number — must still save as a float
    ASSERT_TRUE(cm.Set<bool>("enabled", true).IsOk());
    ASSERT_TRUE(cm.Set<std::string>("basic_str", "world").IsOk());
    ASSERT_TRUE(cm.Set<std::string>("literal_str", "still\\nraw").IsOk());
    ASSERT_TRUE(cm.Set<std::vector<int>>("nums", {9, 8}).IsOk());

    ASSERT_TRUE(cm.Save().IsOk());

    // Inspect the raw saved text — not just the re-parsed values — to
    // confirm string quoting style and float formatting survived.
    auto const savedText = ReadToml();
    EXPECT_NE(savedText.find("count = 2"), std::string::npos);
    EXPECT_NE(savedText.find("pi = 4.0"), std::string::npos);
    EXPECT_NE(savedText.find("enabled = true"), std::string::npos);
    EXPECT_NE(savedText.find(R"(basic_str = "world")"), std::string::npos);
    EXPECT_NE(savedText.find("literal_str = 'still\\nraw'"), std::string::npos);
    EXPECT_NE(savedText.find("nums = [9, 8]"), std::string::npos);

    // Load into a fresh manager instance to confirm the saved file re-parses
    // back to the same values.
    ConfigurationManager reloaded;
    ASSERT_TRUE(reloaded.Load(m_TomlPath).IsOk());

    EXPECT_EQ(reloaded.Get<int>("count").Value(), 2);
    EXPECT_DOUBLE_EQ(reloaded.Get<double>("pi").Value(), 4.0);
    EXPECT_TRUE(reloaded.Get<bool>("enabled").Value());
    EXPECT_EQ(reloaded.Get<std::string>("basic_str").Value(), "world");
    EXPECT_EQ(reloaded.Get<std::string>("literal_str").Value(), "still\\nraw");

    auto nums = reloaded.Get<std::vector<int>>("nums").Value();
    ASSERT_EQ(nums.size(), 2u);
    EXPECT_EQ(nums[0], 9);
    EXPECT_EQ(nums[1], 8);
}

// ─── Concurrency ──────────────────────────────────────────────────────────────
//
// Best-effort smoke test: asserts no crash/torn-read under concurrent
// Get/Set. It cannot prove the absence of data races — ThreadSanitizer in CI
// is the real guarantor of that.
TEST_F(ConfigurationManagerTest, Concurrent_GetDuringSet_NoCorruption)
{
    WriteToml("count = 0\n");

    ConfigurationManager cm;
    ASSERT_TRUE(cm.Load(m_TomlPath).IsOk());

    std::atomic<bool> stop{false};

    std::thread writer([&] {
        for (int i = 0; i < 200; ++i)
        {
            cm.Set<int>("count", i);
        }
        stop.store(true);
    });

    std::thread reader([&] {
        while (!stop.load())
        {
            auto result = cm.Get<int>("count");
            if (result.IsOk())
            {
                EXPECT_GE(result.Value(), 0);
                EXPECT_LT(result.Value(), 200);
            }
        }
    });

    writer.join();
    reader.join();

    auto final_result = cm.Get<int>("count");
    ASSERT_TRUE(final_result.IsOk());
    EXPECT_EQ(final_result.Value(), 199);
}

}
