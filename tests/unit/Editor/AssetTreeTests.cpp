#include "AssetTree.hpp"

#include <gtest/gtest.h>

namespace
{

TEST(ListFolderTest, NoPaths_ListsNothing)
{
    auto const listing = ListFolder({}, "");

    EXPECT_TRUE(listing.m_Folders.empty());
    EXPECT_TRUE(listing.m_Files.empty());
}

TEST(ListFolderTest, TopLevel_SplitsFoldersFromFiles)
{
    auto const listing = ListFolder({ "assets/a.png", "assets/sub/b.png", "other/c.png", "d.png" }, "");

    EXPECT_EQ(listing.m_Folders, (std::vector<std::string>{ "assets", "other" }));
    EXPECT_EQ(listing.m_Files, (std::vector<std::string>{ "d.png" }));
}

TEST(ListFolderTest, Folder_ListsOnlyItsDirectChildren)
{
    auto const listing = ListFolder({ "assets/a.png", "assets/sub/b.png", "assets/sub/deep/c.png", "other/d.png" }, "assets");

    EXPECT_EQ(listing.m_Folders, (std::vector<std::string>{ "sub" }));
    EXPECT_EQ(listing.m_Files, (std::vector<std::string>{ "a.png" }));
}

TEST(ListFolderTest, FolderChain_IsNotMerged)
{
    auto const listing = ListFolder({ "assets/textures/characters/player.png" }, "assets");

    EXPECT_EQ(listing.m_Folders, (std::vector<std::string>{ "textures" }));
    EXPECT_TRUE(listing.m_Files.empty());
}

TEST(ListFolderTest, SiblingWithTheSamePrefix_IsNotInsideTheFolder)
{
    auto const listing = ListFolder({ "assets2/a.png", "assets/b.png" }, "assets");

    EXPECT_TRUE(listing.m_Folders.empty());
    EXPECT_EQ(listing.m_Files, (std::vector<std::string>{ "b.png" }));
}

TEST(ListFolderTest, UnknownFolder_ListsNothing)
{
    auto const listing = ListFolder({ "assets/a.png" }, "missing");

    EXPECT_TRUE(listing.m_Folders.empty());
    EXPECT_TRUE(listing.m_Files.empty());
}

}
