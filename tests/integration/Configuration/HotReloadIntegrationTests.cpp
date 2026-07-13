#include <ASGE/Core/Configuration/ConfigurationManager.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace
{

using namespace asge::config;

class ConfigIntegrationTest : public ::testing::Test
{
protected:
    std::filesystem::path m_TempDir;

    void SetUp() override
    {
        m_TempDir = std::filesystem::temp_directory_path() / 
                    ("asge_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
        std::filesystem::create_directories(m_TempDir);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_TempDir, ec);
    }

    std::filesystem::path WriteConfig(std::string const& contents, std::string const& filename = "config.toml")
    {
        auto path = m_TempDir / filename;
        std::ofstream ofs(path);
        ofs << contents;
        ofs.close();
        return path;
    }
};

template<typename Pred>
bool WaitFor(Pred pred, std::chrono::milliseconds timeout = std::chrono::seconds(2))
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

TEST_F(ConfigIntegrationTest, LoadAndGetTypedValue)
{
    auto path = WriteConfig(R"(
        [graphics]
        width = 1920
        height = 1080
        vsync = true
    )");

    ConfigurationManager mgr;
    auto result = mgr.Load(path);
    ASSERT_TRUE(result.IsOk());

    auto width = mgr.Get<int>("graphics.width");
    ASSERT_TRUE(width.IsOk());
    EXPECT_EQ(width.Value(), 1920);

    auto vsync = mgr.Get<bool>("graphics.vsync");
    ASSERT_TRUE(vsync.IsOk());
    EXPECT_TRUE(vsync.Value());
}

TEST_F(ConfigIntegrationTest, HotReloadPicksUpChange)
{
    auto path = WriteConfig("[graphics]\nwidth = 1920\n");

    ConfigurationManager mgr;
    ASSERT_TRUE(mgr.Load(path).IsOk());
    mgr.SetHotReloadEnabled(true);

    WriteConfig("[graphics]\nwidth = 2560\n"); // overwrite same file

    bool updated = WaitFor([&] {
        auto w = mgr.Get<int>("graphics.width");
        return w.IsOk() && w.Value() == 2560;
    });

    ASSERT_TRUE(updated);
}

TEST_F(ConfigIntegrationTest, RejectsMalformedReloadKeepsLastGood)
{
    auto path = WriteConfig("[graphics]\nwidth = 1920\n");

    ConfigurationManager mgr;
    ASSERT_TRUE(mgr.Load(path).IsOk());
    mgr.SetHotReloadEnabled(true);

    WriteConfig("[graphics\nwidth = broken"); // malformed TOML

    // give the watcher a chance to fire and fail
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto width = mgr.Get<int>("graphics.width");
    ASSERT_TRUE(width.IsOk());
    EXPECT_EQ(width.Value(), 1920); // still last-known-good
}

TEST_F(ConfigIntegrationTest, SaveTriggersHotReloadReparseWithSameSavedContent)
{
    auto path = WriteConfig("[graphics]\nwidth = 1920\n");

    ConfigurationManager mgr;
    ASSERT_TRUE(mgr.Load(path).IsOk());
    mgr.SetHotReloadEnabled(true);

    ASSERT_TRUE(mgr.Set<int>("graphics.width", 2560).IsOk());
    ASSERT_TRUE(mgr.Save().IsOk());

    // Save() writes the file, which fires the watcher and triggers a
    // re-parse of what was just written — expected, not a bug. The value
    // should still reflect what was saved once that reload settles.
    bool updated = WaitFor([&] {
        auto w = mgr.Get<int>("graphics.width");
        return w.IsOk() && w.Value() == 2560;
    });

    ASSERT_TRUE(updated);
}

}
