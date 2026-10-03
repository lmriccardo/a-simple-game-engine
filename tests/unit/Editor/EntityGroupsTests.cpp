#include "EntityGroups.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{

using asge::ecs::Entity;
using asge::game::scene::LoadContext;
using asge::game::scene::SaveContext;

class EntityGroupsTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Scene;

    void SetUp() override
    {
        m_Scene = std::filesystem::temp_directory_path()
            / ("asge_entity_groups_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".asgescene");
        ClearAllGroups();
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(GroupsFilePath(m_Scene), ec);
        ClearAllGroups();
    }
};

TEST_F(EntityGroupsTest, GroupsFor_ReturnsTheSameListForTheSameScene)
{
    GroupsFor("a").push_back({ "Enemies", {}, true });

    EXPECT_EQ(GroupsFor("a").size(), 1u);
    EXPECT_TRUE(GroupsFor("b").empty());
}

TEST_F(EntityGroupsTest, RenameGroupScene_MovesTheGroupsToTheNewPath)
{
    GroupsFor("old").push_back({ "Enemies", {}, true });

    RenameGroupScene("old", "new");

    EXPECT_TRUE(GroupsFor("old").empty());
    ASSERT_EQ(GroupsFor("new").size(), 1u);
    EXPECT_EQ(GroupsFor("new")[0].m_Name, "Enemies");
}

TEST_F(EntityGroupsTest, RemapGroupMembers_SwapsHandlesAndDropsTheOnesNotRestored)
{
    Entity const kept{ 1, 0 }, gone{ 2, 0 }, renamed{ 7, 3 };
    EntityGroupList groups{ { "Group", { kept, gone }, true } };

    RemapGroupMembers(groups, { { kept, renamed } });

    ASSERT_EQ(groups[0].m_Members.size(), 1u);
    EXPECT_EQ(groups[0].m_Members[0], renamed);
}

TEST_F(EntityGroupsTest, SaveThenLoad_RoundTripsNamesAndMembersThroughFileIndices)
{
    Entity const a{ 10, 0 }, b{ 11, 0 }, c{ 12, 0 };
    EntityGroupList groups{ { "Enemies", { a, c }, true }, { "Empty", {}, true } };

    SaveContext saveCtx;
    saveCtx.m_Ids = { { a, 0u }, { b, 1u }, { c, 2u } };
    ASSERT_TRUE(SaveGroupsFile(m_Scene, groups, saveCtx).IsOk());

    // After a reload, file indices lead to different (new) entities.
    Entity const newA{ 1, 5 }, newC{ 3, 5 };
    LoadContext loadCtx;
    loadCtx.m_Entities = { { 0u, newA }, { 2u, newC } };
    auto const loaded = LoadGroupsFile(m_Scene, loadCtx);

    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].m_Name, "Enemies");
    ASSERT_EQ(loaded[0].m_Members.size(), 2u);
    EXPECT_EQ(loaded[0].m_Members[0], newA);
    EXPECT_EQ(loaded[0].m_Members[1], newC);
    EXPECT_EQ(loaded[1].m_Name, "Empty");
    EXPECT_TRUE(loaded[1].m_Members.empty());
}

TEST_F(EntityGroupsTest, RemapGroupMembers_RemapsTheParentAndDropsGroupsWhoseParentIsGone)
{
    Entity const parent{ 1, 0 }, orphanParent{ 2, 0 }, child{ 3, 0 }, newParent{ 8, 1 }, newChild{ 9, 1 };
    EntityGroupList groups{ { "Kids", { child }, true, parent }, { "Lost", { child }, true, orphanParent } };

    RemapGroupMembers(groups, { { parent, newParent }, { child, newChild } });

    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].m_Parent, newParent);
    ASSERT_EQ(groups[0].m_Members.size(), 1u);
    EXPECT_EQ(groups[0].m_Members[0], newChild);
}

TEST_F(EntityGroupsTest, SaveThenLoad_KeepsAChildGroupUnderItsParent)
{
    Entity const parent{ 10, 0 }, child{ 11, 0 };
    EntityGroupList groups{ { "Top", {}, true }, { "Kids", { child }, true, parent } };

    SaveContext saveCtx;
    saveCtx.m_Ids = { { parent, 0u }, { child, 1u } };
    ASSERT_TRUE(SaveGroupsFile(m_Scene, groups, saveCtx).IsOk());

    Entity const newParent{ 4, 2 }, newChild{ 5, 2 };
    LoadContext loadCtx;
    loadCtx.m_Entities = { { 0u, newParent }, { 1u, newChild } };
    auto const loaded = LoadGroupsFile(m_Scene, loadCtx);

    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].m_Parent, Entity::Null()); // the top-level group stays top level
    EXPECT_EQ(loaded[1].m_Parent, newParent);
    ASSERT_EQ(loaded[1].m_Members.size(), 1u);
    EXPECT_EQ(loaded[1].m_Members[0], newChild);
}

TEST_F(EntityGroupsTest, SaveThenLoad_SkipsAGroupWhoseParentIsNotInTheScene)
{
    Entity const stranger{ 50, 0 };
    SaveContext saveCtx; // knows nothing about the parent
    ASSERT_TRUE(SaveGroupsFile(m_Scene, { { "Kids", {}, true, stranger } }, saveCtx).IsOk());

    EXPECT_TRUE(LoadGroupsFile(m_Scene, LoadContext{}).empty());
}

TEST_F(EntityGroupsTest, SaveGroupsFile_WithNoGroups_RemovesTheFile)
{
    SaveContext ctx;
    ASSERT_TRUE(SaveGroupsFile(m_Scene, { { "Group", {}, true } }, ctx).IsOk());
    ASSERT_TRUE(std::filesystem::exists(GroupsFilePath(m_Scene)));

    ASSERT_TRUE(SaveGroupsFile(m_Scene, {}, ctx).IsOk());

    EXPECT_FALSE(std::filesystem::exists(GroupsFilePath(m_Scene)));
}

TEST_F(EntityGroupsTest, LoadGroupsFile_MissingFile_GivesNoGroups)
{
    EXPECT_TRUE(LoadGroupsFile(m_Scene, LoadContext{}).empty());
}

}
