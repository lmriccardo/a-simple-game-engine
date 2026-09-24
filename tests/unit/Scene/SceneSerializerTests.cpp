#include <ASGE/Game/Scene/SceneSerializer.hpp>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Components.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace
{

using namespace asge::game::scene;
using namespace asge::game::components;
using asge::config::toml::TOMLTableView;

class SceneSerializerTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Root;
    std::filesystem::path m_ScenePath;
    asge::filesystem::VirtualFileSystem m_Vfs;
    asge::ecs::Registry m_Registry;

    void SetUp() override
    {
        auto const uniqueName = "asge_scene_serializer_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this));
        m_Root = std::filesystem::temp_directory_path() / uniqueName;
        m_ScenePath = m_Root / "scene.toml";

        std::filesystem::create_directories(m_Root);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Root, ec);
    }

    // Parses the file just written back into a table tree, wrapping the
    // root in a TOMLTableView so the test can walk it — via GetTable(),
    // which (unlike Table()) honors "[index]" segments — the same way a
    // real loader would descend into entity[0], entity[1], ...
    static TOMLTableView ParseSavedFile( std::filesystem::path const& inPath )
    {
        auto const content = asge::filesystem::ReadText( inPath );
        EXPECT_TRUE(content.IsOk());

        auto table = asge::config::toml::Parse( content.Value() );
        EXPECT_TRUE(table.IsOk());
        return TOMLTableView( table.Value() );
    }
};

// ─── Save ───────────────────────────────────────────────────────────────────

TEST_F(SceneSerializerTest, Save_EmptyRegistry_WritesFileWithNoEntityTables)
{
    SceneSerializer serializer{ m_Vfs };
    auto result = serializer.Save( m_Registry, m_ScenePath );

    ASSERT_TRUE(result.IsOk());
    ASSERT_TRUE(std::filesystem::exists(m_ScenePath));

    auto root = ParseSavedFile( m_ScenePath );
    EXPECT_FALSE(root.HasTable("entity[0]"));
}

TEST_F(SceneSerializerTest, Save_EntityWithNoComponents_WritesEntityTableWithNoComponentSubtables)
{
    ASSERT_TRUE(m_Registry.CreateEntity().IsOk());

    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());

    auto root = ParseSavedFile( m_ScenePath );
    ASSERT_TRUE(root.HasTable("entity[0]"));

    auto entity = root.GetTable("entity[0]").Value();
    EXPECT_FALSE(entity.HasTable("Transform"));
    EXPECT_FALSE(entity.HasTable("Velocity"));
    EXPECT_FALSE(entity.HasTable("Sprite"));
}

TEST_F(SceneSerializerTest, Save_EntityWithSubsetOfComponents_WritesOnlyThoseSubtablesWithFieldsIntact)
{
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());

    Transform const transform{ .m_LocalCoordinates = {1.0f, 2.0f}, .m_LocalScale = {3.0f, 4.0f}, .m_LocalRotation = 0.5f };
    Sprite sprite{};
    sprite.m_VirtualPath = "textures/checker.bmp";

    ASSERT_TRUE(m_Registry.AddComponent( entity.Value(), transform ).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent( entity.Value(), sprite ).IsOk());
    // Deliberately no Velocity.

    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());

    auto root = ParseSavedFile( m_ScenePath );
    auto entityView = root.GetTable("entity[0]").Value();

    EXPECT_TRUE(entityView.HasTable("Transform"));
    EXPECT_TRUE(entityView.HasTable("Sprite"));
    EXPECT_FALSE(entityView.HasTable("Velocity"));

    Transform const restoredTransform = Serializer<Transform>::FromToml( entityView, asge::game::scene::LoadContext{} );
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalCoordinates.x(), transform.m_LocalCoordinates.x());
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalCoordinates.y(), transform.m_LocalCoordinates.y());
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalRotation, transform.m_LocalRotation);
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalScale.x(), transform.m_LocalScale.x());
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalScale.y(), transform.m_LocalScale.y());

    Sprite const restoredSprite = Serializer<Sprite>::FromToml(entityView, asge::game::scene::LoadContext{});
    EXPECT_EQ(restoredSprite.m_VirtualPath, sprite.m_VirtualPath);
}

TEST_F(SceneSerializerTest, Save_MultipleEntities_WritesOneArrayTableEachInCreationOrder)
{
    auto first = m_Registry.CreateEntity();
    auto second = m_Registry.CreateEntity();
    auto third = m_Registry.CreateEntity();
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(second.IsOk());
    ASSERT_TRUE(third.IsOk());

    ASSERT_TRUE(m_Registry.AddComponent( first.Value(), Velocity{ 1.0f, 0.0f } ).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent( third.Value(), Velocity{ 3.0f, 0.0f } ).IsOk());
    // second has no components at all.

    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());

    auto root = ParseSavedFile( m_ScenePath );

    auto firstView = root.GetTable("entity[0]").Value();
    EXPECT_TRUE(firstView.HasTable("Velocity"));
    EXPECT_FLOAT_EQ(Serializer<Velocity>::FromToml(firstView, asge::game::scene::LoadContext{}).m_DX, 1.0f);

    auto secondView = root.GetTable("entity[1]").Value();
    EXPECT_FALSE(secondView.HasTable("Velocity"));

    auto thirdView = root.GetTable("entity[2]").Value();
    EXPECT_TRUE(thirdView.HasTable("Velocity"));
    EXPECT_FLOAT_EQ(Serializer<Velocity>::FromToml(thirdView, asge::game::scene::LoadContext{}).m_DX, 3.0f);

    EXPECT_FALSE(root.HasTable("entity[3]"));
}

TEST_F(SceneSerializerTest, Save_ToDirectoryThatDoesNotExistReturnsError)
{
    ASSERT_TRUE(m_Registry.CreateEntity().IsOk());

    SceneSerializer serializer{ m_Vfs };
    auto result = serializer.Save( m_Registry, m_Root / "missing_subdir" / "scene.toml" );

    EXPECT_FALSE(result.IsOk());
}

TEST_F(SceneSerializerTest, Save_OverwritesAPreviouslySavedFile)
{
    auto firstEntity = m_Registry.CreateEntity();
    ASSERT_TRUE(firstEntity.IsOk());
    ASSERT_TRUE(m_Registry.AddComponent( firstEntity.Value(), Velocity{ 9.0f, 9.0f } ).IsOk());

    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());

    // A fresh registry with different contents, saved to the same path.
    asge::ecs::Registry freshRegistry;
    ASSERT_TRUE(freshRegistry.CreateEntity().IsOk());
    ASSERT_TRUE(serializer.Save( freshRegistry, m_ScenePath ).IsOk());

    auto root = ParseSavedFile( m_ScenePath );
    auto entityView = root.GetTable("entity[0]").Value();
    EXPECT_FALSE(entityView.HasTable("Velocity"));
    EXPECT_FALSE(root.HasTable("entity[1]"));
}

// ─── Load ───────────────────────────────────────────────────────────────────

TEST_F(SceneSerializerTest, Load_ValidSceneFile_RecreatesEntitiesWithSavedComponentsInCreationOrder)
{
    auto withTransform = m_Registry.CreateEntity();
    auto withVelocity = m_Registry.CreateEntity();
    auto bare = m_Registry.CreateEntity();
    ASSERT_TRUE(withTransform.IsOk());
    ASSERT_TRUE(withVelocity.IsOk());
    ASSERT_TRUE(bare.IsOk());

    Transform const transform{ .m_LocalCoordinates = {1.0f, 2.0f}, .m_LocalScale = {3.0f, 4.0f}, .m_LocalRotation = 0.5f };
    Velocity const velocity{ 5.0f, 6.0f };
    ASSERT_TRUE(m_Registry.AddComponent( withTransform.Value(), transform ).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent( withVelocity.Value(), velocity ).IsOk());

    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());
    ASSERT_TRUE(m_Vfs.Mount("scenes", m_Root.string()).IsOk());

    asge::ecs::Registry loaded;
    ASSERT_TRUE(serializer.Load( loaded, "scenes/scene.toml" ).IsOk());

    // AllEntities() walks slots in allocation order, and a fresh Registry
    // hands out CreateEntity() indices sequentially -- so loaded's entities
    // land back in the same order they were saved (see Save's own
    // Save_MultipleEntities_WritesOneArrayTableEachInCreationOrder, which
    // relies on the same ordering guarantee).
    auto all = loaded.AllEntities();
    ASSERT_EQ(all.size(), 3u);

    ASSERT_TRUE(loaded.HasComponent<Transform>(all[0]));
    EXPECT_FALSE(loaded.HasComponent<Velocity>(all[0]));
    auto restoredTransformResult = loaded.GetComponent<Transform>(all[0]);
    Transform const& restoredTransform = restoredTransformResult.Value().get();
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalCoordinates.x(), transform.m_LocalCoordinates.x());
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalCoordinates.y(), transform.m_LocalCoordinates.y());
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalRotation, transform.m_LocalRotation);
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalScale.x(), transform.m_LocalScale.x());
    EXPECT_FLOAT_EQ(restoredTransform.m_LocalScale.y(), transform.m_LocalScale.y());

    ASSERT_TRUE(loaded.HasComponent<Velocity>(all[1]));
    auto restoredVelocityResult = loaded.GetComponent<Velocity>(all[1]);
    Velocity const& restoredVelocity = restoredVelocityResult.Value().get();
    EXPECT_FLOAT_EQ(restoredVelocity.m_DX, velocity.m_DX);
    EXPECT_FLOAT_EQ(restoredVelocity.m_DY, velocity.m_DY);

    EXPECT_FALSE(loaded.HasComponent<Transform>(all[2]));
    EXPECT_FALSE(loaded.HasComponent<Velocity>(all[2]));
    EXPECT_FALSE(loaded.HasComponent<Sprite>(all[2]));
}

TEST_F(SceneSerializerTest, Load_SpriteComponent_RestoresPathAndSourceRectButLeavesTextureNull)
{
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());

    Sprite sprite{};
    sprite.m_VirtualPath = "textures/checker.bmp";
    sprite.m_SourceRect = asge::math::Rect{ 1.0f, 2.0f, 3.0f, 4.0f };
    ASSERT_TRUE(m_Registry.AddComponent( entity.Value(), sprite ).IsOk());

    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());
    ASSERT_TRUE(m_Vfs.Mount("scenes", m_Root.string()).IsOk());

    asge::ecs::Registry loaded;
    ASSERT_TRUE(serializer.Load( loaded, "scenes/scene.toml" ).IsOk());

    auto all = loaded.AllEntities();
    ASSERT_EQ(all.size(), 1u);
    ASSERT_TRUE(loaded.HasComponent<Sprite>(all[0]));

    auto restoredResult = loaded.GetComponent<Sprite>(all[0]);
    Sprite const& restored = restoredResult.Value().get();
    EXPECT_EQ(restored.m_VirtualPath, "textures/checker.bmp");
    EXPECT_EQ(restored.m_Texture, nullptr); // resolving it is the caller's job, not Load's
    ASSERT_TRUE(restored.m_SourceRect.has_value());
    EXPECT_FLOAT_EQ(restored.m_SourceRect->m_X, 1.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->m_Y, 2.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->m_Width, 3.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->m_Height, 4.0f);
}

TEST_F(SceneSerializerTest, Load_EmptySceneFile_ResultsInEmptyRegistry)
{
    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk()); // m_Registry has no entities
    ASSERT_TRUE(m_Vfs.Mount("scenes", m_Root.string()).IsOk());

    asge::ecs::Registry loaded;
    ASSERT_TRUE(serializer.Load( loaded, "scenes/scene.toml" ).IsOk());
    EXPECT_TRUE(loaded.AllEntities().empty());
}

TEST_F(SceneSerializerTest, Load_UnmountedVirtualPathReturnsError)
{
    SceneSerializer serializer{ m_Vfs }; // nothing mounted
    asge::ecs::Registry loaded;

    auto result = serializer.Load( loaded, "scenes/scene.toml" );
    EXPECT_FALSE(result.IsOk());
}

TEST_F(SceneSerializerTest, Load_FileDoesNotExistReturnsError)
{
    ASSERT_TRUE(m_Vfs.Mount("scenes", m_Root.string()).IsOk()); // mounted, but scene.toml was never written
    SceneSerializer serializer{ m_Vfs };
    asge::ecs::Registry loaded;

    auto result = serializer.Load( loaded, "scenes/scene.toml" );
    EXPECT_FALSE(result.IsOk());
}

TEST_F(SceneSerializerTest, Load_MalformedTomlReturnsErrorAndLeavesRegistryUntouched)
{
    ASSERT_TRUE(asge::filesystem::WriteText( m_ScenePath, "not [ valid toml" ).IsOk());
    ASSERT_TRUE(m_Vfs.Mount("scenes", m_Root.string()).IsOk());

    auto preexisting = m_Registry.CreateEntity();
    ASSERT_TRUE(preexisting.IsOk());

    SceneSerializer serializer{ m_Vfs };
    auto result = serializer.Load( m_Registry, "scenes/scene.toml" );
    EXPECT_FALSE(result.IsOk());

    // A resolve/read/parse failure happens before Load touches the
    // registry at all, so whatever was already there survives untouched.
    auto all = m_Registry.AllEntities();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], preexisting.Value());
}

// ─── LoadFromFile ───────────────────────────────────────────────────────────

TEST_F(SceneSerializerTest, LoadFromFile_ValidSceneFile_RecreatesEntitiesWithSavedComponents)
{
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());

    Velocity const velocity{ 7.0f, 8.0f };
    ASSERT_TRUE(m_Registry.AddComponent( entity.Value(), velocity ).IsOk());

    SceneSerializer serializer{ m_Vfs }; // nothing mounted -- LoadFromFile never needs the VFS
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());

    asge::ecs::Registry loaded;
    ASSERT_TRUE(serializer.LoadFromFile( loaded, m_ScenePath ).IsOk());

    auto all = loaded.AllEntities();
    ASSERT_EQ(all.size(), 1u);
    ASSERT_TRUE(loaded.HasComponent<Velocity>(all[0]));
    EXPECT_FLOAT_EQ(loaded.GetComponent<Velocity>(all[0]).Value().get().m_DX, 7.0f);
    EXPECT_FLOAT_EQ(loaded.GetComponent<Velocity>(all[0]).Value().get().m_DY, 8.0f);
}

TEST_F(SceneSerializerTest, LoadFromFile_FileDoesNotExistReturnsError)
{
    SceneSerializer serializer{ m_Vfs };
    asge::ecs::Registry loaded;

    auto result = serializer.LoadFromFile( loaded, m_Root / "does_not_exist.toml" );
    EXPECT_FALSE(result.IsOk());
}

TEST_F(SceneSerializerTest, LoadFromFile_MalformedTomlReturnsErrorAndLeavesRegistryUntouched)
{
    ASSERT_TRUE(asge::filesystem::WriteText( m_ScenePath, "not [ valid toml" ).IsOk());

    auto preexisting = m_Registry.CreateEntity();
    ASSERT_TRUE(preexisting.IsOk());

    SceneSerializer serializer{ m_Vfs };
    auto result = serializer.LoadFromFile( m_Registry, m_ScenePath );
    EXPECT_FALSE(result.IsOk());

    auto all = m_Registry.AllEntities();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0], preexisting.Value());
}

TEST_F(SceneSerializerTest, Load_MidLoopFailureRollsBackOnlyThisCallsEntitiesLeavingPreexistingOnesIntact)
{
    // Two entities to load.
    ASSERT_TRUE(m_Registry.CreateEntity().IsOk());
    ASSERT_TRUE(m_Registry.CreateEntity().IsOk());

    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());
    ASSERT_TRUE(m_Vfs.Mount("scenes", m_Root.string()).IsOk());

    // Destination registry: fill to exactly one free slot, so Load can
    // create its first entity but fails creating its second.
    asge::ecs::Registry almostFull;
    auto sentinel = almostFull.CreateEntity();
    ASSERT_TRUE(sentinel.IsOk());
    for ( std::size_t i = 1; i < asge::ecs::kMaxEntities - 1; ++i )
    {
        ASSERT_TRUE(almostFull.CreateEntity().IsOk());
    }
    ASSERT_EQ(almostFull.AllEntities().size(), asge::ecs::kMaxEntities - 1);

    auto result = serializer.Load( almostFull, "scenes/scene.toml" );
    EXPECT_FALSE(result.IsOk());

    // Only the one entity Load itself managed to create before failing to
    // create its second is rolled back -- every pre-existing entity,
    // including sentinel, is left exactly as it was.
    auto all = almostFull.AllEntities();
    EXPECT_EQ(all.size(), asge::ecs::kMaxEntities - 1);
    EXPECT_NE(std::find(all.begin(), all.end(), sentinel.Value()), all.end());
}

TEST_F(SceneSerializerTest, Load_HierarchyComponent_ReconnectsParentChildRelationshipsAmongTheNewlyCreatedEntities)
{
    auto root = m_Registry.CreateEntity();
    auto childA = m_Registry.CreateEntity();
    auto childB = m_Registry.CreateEntity();
    ASSERT_TRUE(root.IsOk());
    ASSERT_TRUE(childA.IsOk());
    ASSERT_TRUE(childB.IsOk());

    AttachChild( m_Registry, root.Value(), childA.Value() );
    AttachChild( m_Registry, root.Value(), childB.Value() );

    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Save( m_Registry, m_ScenePath ).IsOk());
    ASSERT_TRUE(m_Vfs.Mount("scenes", m_Root.string()).IsOk());

    // Pre-populate `loaded` with five unrelated entities before Load() runs,
    // so root/childA/childB's counterparts land at indices 5-7 there rather
    // than 0-2 -- guaranteeing they can't possibly equal m_Registry's own
    // entities by coincidence, and so proving the Hierarchy links checked
    // below only work because FromToml actually resolved ids through
    // LoadContext, not because it copied the original Entity values through
    // untouched.
    asge::ecs::Registry loaded;
    for ( int i = 0; i < 5; ++i )
    {
        ASSERT_TRUE(loaded.CreateEntity().IsOk());
    }

    ASSERT_TRUE(serializer.Load( loaded, "scenes/scene.toml" ).IsOk());

    // AllEntities() walks slots in index order, and Load() creates its
    // entities in the order it encounters entity[N] blocks (root, childA,
    // childB, per Save's own creation-order guarantee) -- so with `loaded`
    // otherwise untouched, they land at the next three free indices in that
    // same order.
    auto const all = loaded.AllEntities();
    ASSERT_EQ(all.size(), 8u);
    auto const loadedRoot = all[5];
    auto const loadedChildA = all[6];
    auto const loadedChildB = all[7];

    // None of the original handles survived the round trip -- confirms the
    // assertions below are only possible because FromToml actually resolved
    // ids through LoadContext, not because it copied the old Entity values.
    EXPECT_NE(loadedRoot, root.Value());
    EXPECT_NE(loadedChildA, childA.Value());
    EXPECT_NE(loadedChildB, childB.Value());

    ASSERT_TRUE(loaded.HasComponent<Hierarchy>(loadedRoot));
    ASSERT_TRUE(loaded.HasComponent<Hierarchy>(loadedChildA));
    ASSERT_TRUE(loaded.HasComponent<Hierarchy>(loadedChildB));

    auto const& rootH = loaded.GetComponent<Hierarchy>(loadedRoot).Value().get();
    EXPECT_EQ(rootH.m_Parent, asge::ecs::Entity::Null());
    EXPECT_EQ(rootH.m_FirstChild, loadedChildA);
    EXPECT_EQ(rootH.m_LastChild, loadedChildB);

    auto const& childAH = loaded.GetComponent<Hierarchy>(loadedChildA).Value().get();
    EXPECT_EQ(childAH.m_Parent, loadedRoot);
    EXPECT_EQ(childAH.m_PrevSibling, asge::ecs::Entity::Null());
    EXPECT_EQ(childAH.m_NextSibling, loadedChildB);

    auto const& childBH = loaded.GetComponent<Hierarchy>(loadedChildB).Value().get();
    EXPECT_EQ(childBH.m_Parent, loadedRoot);
    EXPECT_EQ(childBH.m_PrevSibling, loadedChildA);
    EXPECT_EQ(childBH.m_NextSibling, asge::ecs::Entity::Null());
}

}
