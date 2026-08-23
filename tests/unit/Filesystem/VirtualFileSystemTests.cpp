#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using namespace asge::filesystem;
using asge::errors::VfsError;

class VirtualFileSystemTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Root;
    std::filesystem::path m_AssetsDir;
    std::filesystem::path m_ModDir;

    void SetUp() override
    {
        auto const uniqueName = "asge_vfs_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this));
        m_Root = std::filesystem::temp_directory_path() / uniqueName;
        m_AssetsDir = m_Root / "assets" / "textures";
        m_ModDir = m_Root / "mods" / "textures";

        std::filesystem::create_directories(m_AssetsDir);
        std::filesystem::create_directories(m_ModDir);

        WriteFile(m_AssetsDir / "hero.png", "base-hero");
        WriteFile(m_AssetsDir / "villain.png", "base-villain");
        WriteFile(m_ModDir / "hero.png", "mod-hero");
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

// ─── Mount ──────────────────────────────────────────────────────────────────────

TEST_F(VirtualFileSystemTest, Mount_ValidDirectoryReturnsOk)
{
    VirtualFileSystem vfs;
    auto result = vfs.Mount("textures", m_AssetsDir.string());
    EXPECT_TRUE(result.IsOk());
}

TEST_F(VirtualFileSystemTest, Mount_NonExistentPathReturnsError)
{
    VirtualFileSystem vfs;
    auto result = vfs.Mount("textures", (m_Root / "does_not_exist").string());
    EXPECT_FALSE(result.IsOk());
}

TEST_F(VirtualFileSystemTest, Mount_FileInsteadOfDirectoryReturnsError)
{
    VirtualFileSystem vfs;
    auto result = vfs.Mount("textures", (m_AssetsDir / "hero.png").string());
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::invalid_argument));
}

TEST_F(VirtualFileSystemTest, Mount_SameMountPointAndPathTwiceReturnsAlreadyMountedError)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());

    auto result = vfs.Mount("textures", m_AssetsDir.string());
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::AlreadyMounted));
}

TEST_F(VirtualFileSystemTest, Mount_NormalizesSlashesBeforeDuplicateCheck)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("/textures/", m_AssetsDir.string()).IsOk());

    // Differently-formatted but equivalent mount point must collide with the one above.
    auto result = vfs.Mount("textures", m_AssetsDir.string());
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::AlreadyMounted));
}

TEST_F(VirtualFileSystemTest, Mount_SameMountPointDifferentDirectoriesBothSucceed)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());
    EXPECT_TRUE(vfs.Mount("textures", m_ModDir.string()).IsOk());
}

// ─── Resolve ────────────────────────────────────────────────────────────────────

TEST_F(VirtualFileSystemTest, Resolve_WithNoMountsReturnsEmptyMountsError)
{
    VirtualFileSystem vfs;
    auto result = vfs.Resolve("textures/hero.png");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::EmptyMounts));
}

TEST_F(VirtualFileSystemTest, Resolve_UnknownVirtualRootReturnsNotMountedError)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());

    auto result = vfs.Resolve("audio/theme.ogg");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::NotMounted));
}

TEST_F(VirtualFileSystemTest, Resolve_KnownVirtualPathReturnsRealPath)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());

    auto result = vfs.Resolve("textures/hero.png");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), std::filesystem::canonical(m_AssetsDir / "hero.png"));
}

TEST_F(VirtualFileSystemTest, Resolve_FileMissingFromMountedDirectoryReturnsNotMountedError)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());

    auto result = vfs.Resolve("textures/does_not_exist.png");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::NotMounted));
}

TEST_F(VirtualFileSystemTest, Resolve_ExactMountRootWithNoFileComponentReturnsNotMountedError)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());

    auto result = vfs.Resolve("textures");
    EXPECT_FALSE(result.IsOk());
}

TEST_F(VirtualFileSystemTest, Resolve_ParentTraversalIsRejected)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());

    auto result = vfs.Resolve("textures/../../../etc/passwd");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), std::make_error_code(std::errc::invalid_argument));
}

TEST_F(VirtualFileSystemTest, Resolve_BackslashesAreTreatedAsSeparators)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());

    auto result = vfs.Resolve("textures\\hero.png");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), std::filesystem::canonical(m_AssetsDir / "hero.png"));
}

TEST_F(VirtualFileSystemTest, Resolve_PrefersEarliestRegisteredMountWhenBothHaveTheFile)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk()); // registered first
    ASSERT_TRUE(vfs.Mount("textures", m_ModDir.string()).IsOk());    // registered second

    auto result = vfs.Resolve("textures/hero.png");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), std::filesystem::canonical(m_AssetsDir / "hero.png"));
}

TEST_F(VirtualFileSystemTest, Resolve_FallsBackToLaterMountWhenEarlierOneLacksTheFile)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk()); // no villain.png override here
    ASSERT_TRUE(vfs.Mount("textures", m_ModDir.string()).IsOk());

    // Only the base assets dir has villain.png — resolves through even though
    // the mod mount (registered second) was tried and missed.
    auto result = vfs.Resolve("textures/villain.png");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value(), std::filesystem::canonical(m_AssetsDir / "villain.png"));
}

// ─── Unmount ────────────────────────────────────────────────────────────────────

TEST_F(VirtualFileSystemTest, Unmount_RegisteredMountReturnsOkAndStopsResolving)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());

    auto result = vfs.Unmount("textures", m_AssetsDir.string());
    ASSERT_TRUE(result.IsOk());
    EXPECT_FALSE(vfs.Exists("textures/hero.png"));
}

TEST_F(VirtualFileSystemTest, Unmount_NeverMountedReturnsNotMountedError)
{
    VirtualFileSystem vfs;
    auto result = vfs.Unmount("textures", m_AssetsDir.string());
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::NotMounted));
}

// ─── Exists ─────────────────────────────────────────────────────────────────────

TEST_F(VirtualFileSystemTest, Exists_ResolvablePathReturnsTrue)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());
    EXPECT_TRUE(vfs.Exists("textures/hero.png"));
}

TEST_F(VirtualFileSystemTest, Exists_UnresolvablePathReturnsFalse)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());
    EXPECT_FALSE(vfs.Exists("textures/missing.png"));
    EXPECT_FALSE(vfs.Exists("audio/missing.ogg"));
}

// ─── ListMounts ─────────────────────────────────────────────────────────────────

TEST_F(VirtualFileSystemTest, ListMounts_EmptyByDefault)
{
    VirtualFileSystem vfs;
    EXPECT_TRUE(vfs.ListMounts().empty());
}

TEST_F(VirtualFileSystemTest, ListMounts_ReflectsRegisteredMountsInOrder)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());
    ASSERT_TRUE(vfs.Mount("textures", m_ModDir.string()).IsOk());

    auto const& mounts = vfs.ListMounts();
    ASSERT_EQ(mounts.size(), 2u);
    EXPECT_EQ(mounts[0].m_VirtualRoot, "textures");
    EXPECT_EQ(mounts[0].m_RealDirectory, std::filesystem::canonical(m_AssetsDir));
    EXPECT_EQ(mounts[1].m_VirtualRoot, "textures");
    EXPECT_EQ(mounts[1].m_RealDirectory, std::filesystem::canonical(m_ModDir));
}

TEST_F(VirtualFileSystemTest, ListMounts_ShrinksAfterUnmount)
{
    VirtualFileSystem vfs;
    ASSERT_TRUE(vfs.Mount("textures", m_AssetsDir.string()).IsOk());
    ASSERT_TRUE(vfs.Unmount("textures", m_AssetsDir.string()).IsOk());

    EXPECT_TRUE(vfs.ListMounts().empty());
}

}
