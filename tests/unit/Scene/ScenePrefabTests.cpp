#include <ASGE/Game/Scene/SceneSerializer.hpp>
#include <ASGE/Game/Assets/ScenePrefab.hpp>
#include <ASGE/Game/Assets/AssetErrors.hpp>
#include <ASGE/Core/Formats/TOML/TOML_Builder.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Components.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

using namespace asge::game::scene;
using namespace asge::game::components;
using asge::errors::AssetError;
using asge::game::asset::ScenePrefab;
using asge::ecs::components::Hierarchy;

class ScenePrefabTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Root;
    std::filesystem::path m_PrefabPath;
    asge::filesystem::VirtualFileSystem m_Vfs;
    asge::ecs::Registry m_Registry;

    void SetUp() override
    {
        m_Root = std::filesystem::temp_directory_path()
            / ("asge_scene_prefab_test_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        m_PrefabPath = m_Root / "prefab.toml";
        std::filesystem::create_directories(m_Root);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Root, ec);
    }

    void WritePrefab( std::string const& inText ) const { std::ofstream( m_PrefabPath ) << inText; }

    // Builds a parent -> (root -> child) tree and returns the root of the prefab.
    asge::ecs::Entity BuildTree( asge::ecs::Entity& outChild, asge::ecs::Entity& outParent )
    {
        outParent = m_Registry.CreateEntity().Value();
        auto const root = m_Registry.CreateEntity().Value();
        outChild = m_Registry.CreateEntity().Value();

        EXPECT_TRUE(m_Registry.AddComponent( root, Transform{ .m_LocalCoordinates = {1.0f, 2.0f} } ).IsOk());
        EXPECT_TRUE(m_Registry.AddComponent( outChild, Transform{ .m_LocalCoordinates = {5.0f, 6.0f} } ).IsOk());
        asge::ecs::components::AttachChild( m_Registry, outParent, root );
        asge::ecs::components::AttachChild( m_Registry, root, outChild );
        return root;
    }

    ScenePrefab SaveAndLoadPrefab( asge::ecs::Entity inRoot )
    {
        SceneSerializer serializer{ m_Vfs };
        EXPECT_TRUE(serializer.SavePrefab( m_Registry, inRoot, m_PrefabPath ).IsOk());
        auto prefab = ScenePrefab::Load( m_PrefabPath );
        EXPECT_TRUE(prefab.IsOk());
        return prefab.Value();
    }
};

// ─── ScenePrefab::Load ──────────────────────────────────────────────────────

TEST_F(ScenePrefabTest, Load_NoEntity_FailsWithAtLeastOneEntity)
{
    WritePrefab( "# nothing here\n" );

    auto result = ScenePrefab::Load( m_PrefabPath );
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code( AssetError::ScenePrefab_AtLeastOneEntity ));
}

TEST_F(ScenePrefabTest, Load_TwoRoots_FailsWithTooManyRoots)
{
    WritePrefab( "[[entity]]\nId = 0\n\n[[entity]]\nId = 1\n" );

    auto result = ScenePrefab::Load( m_PrefabPath );
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code( AssetError::ScenePrefab_TooManyRoots ));
}

TEST_F(ScenePrefabTest, Load_RootIsNotTheFirstEntity_FailsWithRootNotFirst)
{
    WritePrefab(
        "[[entity]]\nId = 1\n[entity.Hierarchy]\nm_Parent = 0\n\n"
        "[[entity]]\nId = 0\n[entity.Hierarchy]\nm_Parent = -1\n" );

    auto result = ScenePrefab::Load( m_PrefabPath );
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code( AssetError::ScenePrefab_RootNotFirst ));
}

TEST_F(ScenePrefabTest, Load_SingleEntityWithoutHierarchy_IsValid)
{
    WritePrefab( "[[entity]]\nId = 0\n" );
    EXPECT_TRUE(ScenePrefab::Load( m_PrefabPath ).IsOk());
}

// ─── SavePrefab ─────────────────────────────────────────────────────────────

TEST_F(ScenePrefabTest, SavePrefab_WritesOnlyTheSubtreeWithTheRootFirstAndParentless)
{
    asge::ecs::Entity child, parent;
    auto const root = BuildTree( child, parent );
    ASSERT_TRUE(m_Registry.CreateEntity().IsOk()); // unrelated entity, must not be saved

    SceneSerializer serializer{ m_Vfs };
    SaveContext ctx;
    ASSERT_TRUE(serializer.SavePrefab( m_Registry, root, m_PrefabPath, &ctx ).IsOk());

    EXPECT_EQ(ctx.m_Ids.size(), 2u);
    EXPECT_EQ(ctx.Resolve( root ), 0);
    EXPECT_EQ(ctx.Resolve( child ), 1);
    EXPECT_EQ(ctx.Resolve( parent ), -1);

    // The outside parent is dropped, so the file is a valid single-root prefab
    EXPECT_TRUE(ScenePrefab::Load( m_PrefabPath ).IsOk());
}

// ─── Instantiate ────────────────────────────────────────────────────────────

TEST_F(ScenePrefabTest, Instantiate_RebuildsTheRootAndItsChildWithTheirComponents)
{
    asge::ecs::Entity child, parent;
    auto const prefabRoot = BuildTree( child, parent );
    auto const prefab = SaveAndLoadPrefab( prefabRoot );

    asge::ecs::Registry target;
    SceneSerializer serializer{ m_Vfs };
    auto root = serializer.Instantiate( target, prefab );
    ASSERT_TRUE(root.IsOk());

    EXPECT_EQ(target.AllEntities().size(), 2u);

    auto const& rootT = target.GetComponent<Transform>( root.Value() ).Value().get();
    EXPECT_FLOAT_EQ(rootT.m_LocalCoordinates.x(), 1.0f);

    auto const& h = target.GetComponent<Hierarchy>( root.Value() ).Value().get();
    EXPECT_EQ(h.m_Parent, asge::ecs::Entity::Null());
    ASSERT_NE(h.m_FirstChild, asge::ecs::Entity::Null());
    auto const& childT = target.GetComponent<Transform>( h.m_FirstChild ).Value().get();
    EXPECT_FLOAT_EQ(childT.m_LocalCoordinates.x(), 5.0f);
}

TEST_F(ScenePrefabTest, Instantiate_Twice_CreatesIndependentCopies)
{
    asge::ecs::Entity child, parent;
    auto const prefab = SaveAndLoadPrefab( BuildTree( child, parent ) );

    asge::ecs::Registry target;
    SceneSerializer serializer{ m_Vfs };
    auto a = serializer.Instantiate( target, prefab );
    auto b = serializer.Instantiate( target, prefab );
    ASSERT_TRUE(a.IsOk());
    ASSERT_TRUE(b.IsOk());

    EXPECT_NE(a.Value(), b.Value());
    EXPECT_EQ(target.AllEntities().size(), 4u);

    target.GetComponent<Transform>( a.Value() ).Value().get().m_LocalCoordinates = {99.0f, 0.0f};
    EXPECT_FLOAT_EQ(target.GetComponent<Transform>( b.Value() ).Value().get().m_LocalCoordinates.x(), 1.0f);
}

TEST_F(ScenePrefabTest, Instantiate_WithTransform_ReplacesOnlyTheRootTransform)
{
    asge::ecs::Entity child, parent;
    auto const prefab = SaveAndLoadPrefab( BuildTree( child, parent ) );

    asge::ecs::Registry target;
    SceneSerializer serializer{ m_Vfs };
    auto root = serializer.Instantiate( target, prefab, Transform{ .m_LocalCoordinates = {40.0f, 50.0f} } );
    ASSERT_TRUE(root.IsOk());

    EXPECT_FLOAT_EQ(target.GetComponent<Transform>( root.Value() ).Value().get().m_LocalCoordinates.x(), 40.0f);

    auto const firstChild = target.GetComponent<Hierarchy>( root.Value() ).Value().get().m_FirstChild;
    EXPECT_FLOAT_EQ(target.GetComponent<Transform>( firstChild ).Value().get().m_LocalCoordinates.x(), 5.0f);
}

TEST_F(ScenePrefabTest, Instantiate_IntoANonEmptyRegistry_LeavesExistingEntitiesUntouched)
{
    asge::ecs::Entity child, parent;
    auto const prefab = SaveAndLoadPrefab( BuildTree( child, parent ) );

    asge::ecs::Registry target;
    auto const existing = target.CreateEntity().Value();
    SceneSerializer serializer{ m_Vfs };
    ASSERT_TRUE(serializer.Instantiate( target, prefab ).IsOk());

    EXPECT_EQ(target.AllEntities().size(), 3u);
    EXPECT_FALSE(target.HasComponent<Transform>( existing ));
}

} // namespace
