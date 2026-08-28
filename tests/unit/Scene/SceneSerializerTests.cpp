#include <ASGE/Game/Scene/SceneSerializer.hpp>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Components.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace
{

using namespace asge::game::scene;
using namespace asge::game::components;
using asge::config::TOMLTableView;

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

        auto table = asge::config::_internal::toml::Parse( content.Value() );
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

    Transform const transform{ 1.0f, 2.0f, 0.5f, 3.0f, 4.0f };
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

    Transform const restoredTransform = Serializer<Transform>::FromToml( entityView );
    EXPECT_FLOAT_EQ(restoredTransform.m_X, transform.m_X);
    EXPECT_FLOAT_EQ(restoredTransform.m_Y, transform.m_Y);
    EXPECT_FLOAT_EQ(restoredTransform.m_Rotation, transform.m_Rotation);
    EXPECT_FLOAT_EQ(restoredTransform.m_ScaleX, transform.m_ScaleX);
    EXPECT_FLOAT_EQ(restoredTransform.m_ScaleY, transform.m_ScaleY);

    Sprite const restoredSprite = Serializer<Sprite>::FromToml( entityView );
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
    EXPECT_FLOAT_EQ(Serializer<Velocity>::FromToml( firstView ).m_DX, 1.0f);

    auto secondView = root.GetTable("entity[1]").Value();
    EXPECT_FALSE(secondView.HasTable("Velocity"));

    auto thirdView = root.GetTable("entity[2]").Value();
    EXPECT_TRUE(thirdView.HasTable("Velocity"));
    EXPECT_FLOAT_EQ(Serializer<Velocity>::FromToml( thirdView ).m_DX, 3.0f);

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

}
