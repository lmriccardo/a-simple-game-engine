#include "AssetTree.hpp"

#include <gtest/gtest.h>

namespace
{

TEST(AssetTreeTest, NoPaths_GivesAnEmptyTree)
{
    EXPECT_TRUE(BuildAssetTree({}).empty());
}

TEST(AssetTreeTest, PathWithoutFolders_IsAFileAtTheRoot)
{
    auto const tree = BuildAssetTree({ "player.png" });

    ASSERT_EQ(tree.size(), 1u);
    EXPECT_FALSE(tree[0].m_IsFolder);
    EXPECT_EQ(tree[0].m_Name, "player.png");
    EXPECT_EQ(tree[0].m_Path, "player.png");
}

TEST(AssetTreeTest, LoneFolderChain_IsMergedIntoOneRow)
{
    auto const tree = BuildAssetTree({ "assets/textures/characters/player.png" });

    ASSERT_EQ(tree.size(), 1u);
    EXPECT_TRUE(tree[0].m_IsFolder);
    EXPECT_EQ(tree[0].m_Name, "assets/textures/characters");
    EXPECT_EQ(tree[0].m_Path, "assets/textures/characters");
    ASSERT_EQ(tree[0].m_Children.size(), 1u);
    EXPECT_EQ(tree[0].m_Children[0].m_Name, "player.png");
    EXPECT_EQ(tree[0].m_Children[0].m_Path, "assets/textures/characters/player.png");
}

TEST(AssetTreeTest, FolderWithSeveralChildren_StaysSplitFromThatPointOn)
{
    auto const tree = BuildAssetTree({ "assets/textures/a/x.png", "assets/textures/b/y.png" });

    ASSERT_EQ(tree.size(), 1u);
    EXPECT_EQ(tree[0].m_Name, "assets/textures"); // the shared prefix merges...
    ASSERT_EQ(tree[0].m_Children.size(), 2u);     // ...then it branches
    EXPECT_EQ(tree[0].m_Children[0].m_Name, "a");
    EXPECT_EQ(tree[0].m_Children[0].m_Path, "assets/textures/a");
    EXPECT_EQ(tree[0].m_Children[1].m_Name, "b");
}

TEST(AssetTreeTest, FolderHoldingAFileAndAFolder_IsNotMerged)
{
    auto const tree = BuildAssetTree({ "assets/readme.png", "assets/sub/x.png" });

    ASSERT_EQ(tree.size(), 1u);
    EXPECT_EQ(tree[0].m_Name, "assets");
    ASSERT_EQ(tree[0].m_Children.size(), 2u);
    EXPECT_TRUE(tree[0].m_Children[0].m_IsFolder); // folders first
    EXPECT_EQ(tree[0].m_Children[0].m_Name, "sub");
    EXPECT_FALSE(tree[0].m_Children[1].m_IsFolder);
    EXPECT_EQ(tree[0].m_Children[1].m_Name, "readme.png");
}

TEST(AssetTreeTest, SiblingsAreAlphabeticalWithFoldersBeforeFiles)
{
    auto const tree = BuildAssetTree({ "z.png", "b/x.png", "b/y.png", "a/x.png", "a/y.png", "m.png" });

    ASSERT_EQ(tree.size(), 4u);
    EXPECT_EQ(tree[0].m_Name, "a");
    EXPECT_EQ(tree[1].m_Name, "b");
    EXPECT_EQ(tree[2].m_Name, "m.png");
    EXPECT_EQ(tree[3].m_Name, "z.png");
}

TEST(AssetTreeTest, EveryPathAppearsExactlyOnceAsAFile)
{
    std::set<std::string> const paths{ "assets/a.png", "assets/sub/b.png", "assets/sub/deep/c.png", "d.png" };

    std::set<std::string> found;
    auto const collect = [&](auto const& self, std::vector<AssetTreeNode> const& inNodes) -> void
    {
        for (auto const& node : inNodes)
        {
            if (node.m_IsFolder) self(self, node.m_Children);
            else EXPECT_TRUE(found.insert(node.m_Path).second);
        }
    };
    collect(collect, BuildAssetTree(paths));

    EXPECT_EQ(found, paths);
}

}
