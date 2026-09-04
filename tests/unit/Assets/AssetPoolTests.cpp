#include <ASGE/Game/Assets/AssetPool.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using namespace asge::game::asset;
using asge::errors::VfsError;

class AssetPoolTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Root;

    void SetUp() override
    {
        auto const uniqueName = "asge_assetpool_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this));
        m_Root = std::filesystem::temp_directory_path() / uniqueName;
        std::filesystem::create_directories(m_Root);

        WriteFile(m_Root / "thing.dat", "content");
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Root, ec);
    }

    static void WriteFile(std::filesystem::path const& inPath, std::string const& inContent)
    {
        std::ofstream file(inPath, std::ios::trunc | std::ios::binary);
        file << inContent;
    }
};

// ─── GetOrLoad — VFS resolve failure ───────────────────────────────────────────

TEST_F(AssetPoolTest, GetOrLoad_UnresolvableVirtualPathReturnsErrorWithoutCallingLoader)
{
    asge::filesystem::VirtualFileSystem vfs; // no mounts registered
    int loadCount = 0;

    AssetPool<int> pool( [&](asge::filesystem::Path const&) -> asge::Result<int>
    {
        ++loadCount;
        return asge::Result<int>::Ok(1);
    });

    auto result = pool.GetOrLoad(vfs, "assets/thing.dat");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::EmptyMounts));
    EXPECT_EQ(loadCount, 0);
}

// ─── GetOrLoad — success + caching ──────────────────────────────────────────────

TEST_F(AssetPoolTest, GetOrLoad_ValidPathCallsLoaderOnceAndCachesTheResult)
{
    asge::filesystem::VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("assets", m_Root.string()).IsOk());
    int loadCount = 0;

    AssetPool<int> pool( [&](asge::filesystem::Path const&) -> asge::Result<int>
    {
        ++loadCount;
        return asge::Result<int>::Ok(42);
    });

    auto first = pool.GetOrLoad(vfs, "assets/thing.dat");
    ASSERT_TRUE(first.IsOk());
    EXPECT_EQ(first.Value()->Get(), 42);
    EXPECT_EQ(first.Value()->VirtualPath(), "assets/thing.dat");
    EXPECT_EQ(loadCount, 1);

    auto second = pool.GetOrLoad(vfs, "assets/thing.dat");
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(second.Value(), first.Value()); // same cached shared_ptr
    EXPECT_EQ(loadCount, 1);                  // loader not called again
}

TEST_F(AssetPoolTest, GetOrLoad_LoaderFailurePropagatesErrorAndDoesNotCache)
{
    asge::filesystem::VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("assets", m_Root.string()).IsOk());
    int loadCount = 0;
    auto const parseError = std::make_error_code(std::errc::invalid_argument);

    AssetPool<int> pool( [&](asge::filesystem::Path const&) -> asge::Result<int>
    {
        ++loadCount;
        return asge::Result<int>::Err(parseError);
    });

    auto first = pool.GetOrLoad(vfs, "assets/thing.dat");
    ASSERT_FALSE(first.IsOk());
    EXPECT_EQ(first.Code(), parseError);
    EXPECT_EQ(loadCount, 1);

    // A failed load isn't cached -- retrying calls the loader again.
    auto second = pool.GetOrLoad(vfs, "assets/thing.dat");
    ASSERT_FALSE(second.IsOk());
    EXPECT_EQ(loadCount, 2);
}

// ─── GetOrLoad — key args ───────────────────────────────────────────────────────

TEST_F(AssetPoolTest, GetOrLoad_DifferentKeyArgsAreSeparateCacheEntries)
{
    asge::filesystem::VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("assets", m_Root.string()).IsOk());
    int loadCount = 0;

    AssetPool<int, int> pool( [&](asge::filesystem::Path const&, int inSize) -> asge::Result<int>
    {
        ++loadCount;
        return asge::Result<int>::Ok(inSize);
    });

    // Not named small/large -- <windows.h> #defines both as macros (old MIDL
    // type aliases), which silently mangles "auto small = ..." on MSVC.
    auto smallResult = pool.GetOrLoad(vfs, "assets/thing.dat", 16);
    auto largeResult = pool.GetOrLoad(vfs, "assets/thing.dat", 32);
    ASSERT_TRUE(smallResult.IsOk());
    ASSERT_TRUE(largeResult.IsOk());
    EXPECT_EQ(smallResult.Value()->Get(), 16);
    EXPECT_EQ(largeResult.Value()->Get(), 32);
    EXPECT_NE(smallResult.Value(), largeResult.Value());
    EXPECT_EQ(loadCount, 2);

    // Same path + same key arg as an earlier call -- still cached.
    auto smallAgain = pool.GetOrLoad(vfs, "assets/thing.dat", 16);
    ASSERT_TRUE(smallAgain.IsOk());
    EXPECT_EQ(smallAgain.Value(), smallResult.Value());
    EXPECT_EQ(loadCount, 2);
}

}
