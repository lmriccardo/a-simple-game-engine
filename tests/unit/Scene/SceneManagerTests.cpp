#include <ASGE/Game/Scene/SceneManager.hpp>
#include <ASGE/Game/Scene/SceneSerializer.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Components.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace
{

using namespace asge::game::scene;
using asge::game::components::Velocity;
using asge::errors::EcsError;
using asge::errors::SceneError;

class SceneManagerTest : public ::testing::Test
{
protected:
    std::filesystem::path m_Root;
    std::filesystem::path m_ScenePath;
    asge::filesystem::VirtualFileSystem m_Vfs;

    void SetUp() override
    {
        auto const uniqueName = "asge_scene_manager_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this));
        m_Root = std::filesystem::temp_directory_path() / uniqueName;
        m_ScenePath = m_Root / "scene.toml";

        std::filesystem::create_directories(m_Root);
        ASSERT_TRUE(m_Vfs.Mount("scenes", m_Root.string()).IsOk());
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_Root, ec);
    }

    // Writes a minimal valid scene file (one entity with a Velocity) via
    // SceneSerializer::Save, so tests don't need to hand-author TOML.
    void WriteValidScene( std::filesystem::path const& inPath, float inDX = 1.0f )
    {
        asge::ecs::Registry seed;
        auto entity = seed.CreateEntity();
        ASSERT_TRUE(entity.IsOk());
        ASSERT_TRUE(seed.AddComponent( entity.Value(), Velocity{ inDX, 0.0f } ).IsOk());

        SceneSerializer serializer{ m_Vfs };
        ASSERT_TRUE(serializer.Save( seed, inPath ).IsOk());
    }
};

// ─── LoadScene ──────────────────────────────────────────────────────────────

TEST_F(SceneManagerTest, LoadScene_ValidFile_PopulatesActiveEntitiesAndRecordsPath)
{
    WriteValidScene(m_ScenePath, 5.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 5.0f);

    ASSERT_TRUE(manager.CurrentScenePath().has_value());
    EXPECT_EQ(*manager.CurrentScenePath(), "scenes/scene.toml");
}

TEST_F(SceneManagerTest, LoadScene_InvalidPathLeavesActiveSceneUntouched)
{
    WriteValidScene(m_ScenePath, 5.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    auto result = manager.LoadScene("scenes/does_not_exist.toml");
    EXPECT_FALSE(result.IsOk());

    // The failed load never touched the scene already active -- not even
    // by suspending it into a snapshot before finding out the new load fails.
    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 5.0f);
    ASSERT_TRUE(manager.CurrentScenePath().has_value());
    EXPECT_EQ(*manager.CurrentScenePath(), "scenes/scene.toml");
    EXPECT_EQ(manager.CachedSceneCount(), 0u); // never suspended
}

// ─── LoadSceneFromFile ──────────────────────────────────────────────────────

TEST_F(SceneManagerTest, LoadSceneFromFile_ValidFile_PopulatesActiveEntitiesAndRecordsPathAsString)
{
    WriteValidScene(m_ScenePath, 5.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadSceneFromFile(m_ScenePath).IsOk());

    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 5.0f);

    ASSERT_TRUE(manager.CurrentScenePath().has_value());
    EXPECT_EQ(*manager.CurrentScenePath(), m_ScenePath.string());
}

TEST_F(SceneManagerTest, LoadSceneFromFile_InvalidPathLeavesActiveSceneUntouched)
{
    WriteValidScene(m_ScenePath, 5.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadSceneFromFile(m_ScenePath).IsOk());

    auto result = manager.LoadSceneFromFile(m_Root / "does_not_exist.toml");
    EXPECT_FALSE(result.IsOk());

    // The failed load never touched the scene already active -- not even
    // by suspending it into a snapshot before finding out the new load fails.
    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 5.0f);
    ASSERT_TRUE(manager.CurrentScenePath().has_value());
    EXPECT_EQ(*manager.CurrentScenePath(), m_ScenePath.string());
    EXPECT_EQ(manager.CachedSceneCount(), 0u); // never suspended
}

TEST_F(SceneManagerTest, LoadSceneFromFile_AlreadyActiveSceneIsANoOp)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadSceneFromFile(m_ScenePath).IsOk());

    ASSERT_TRUE(manager.LoadSceneFromFile(m_ScenePath).IsOk());
    EXPECT_EQ(manager.CachedSceneCount(), 0u); // never became "resident but inactive" against itself
}

TEST_F(SceneManagerTest, LoadSceneFromFile_SwapAwayAndBackPreservesLiveStateInsteadOfReloadingFromDisk)
{
    WriteValidScene(m_ScenePath, 5.0f); // on disk: m_DX == 5.0
    auto const otherPath = m_Root / "other.toml";
    WriteValidScene(otherPath, 1.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadSceneFromFile(m_ScenePath).IsOk());

    // Mutate the live entity to a value that exists only in memory, never
    // written back to scene.toml.
    auto entity = manager.ActiveEntities().at(0);
    manager.GetRegistry().GetComponent<Velocity>(entity).Value().get().m_DX = 999.0f;

    ASSERT_TRUE(manager.LoadSceneFromFile(otherPath).IsOk());
    EXPECT_EQ(manager.CachedSceneCount(), 1u); // scene.toml still resident, just not active

    ASSERT_TRUE(manager.LoadSceneFromFile(m_ScenePath).IsOk());
    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 999.0f);
}

TEST_F(SceneManagerTest, LoadSceneFromFile_SameSceneLoadedViaVirtualPathIsATrackedAsADistinctScene)
{
    WriteValidScene(m_ScenePath, 5.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk()); // tagged with the virtual path

    // Same file on disk, but requested by its real path -- a distinct
    // SceneId, so this is treated as a second, unrelated load rather than a
    // residency hit against the virtual-path scene.
    ASSERT_TRUE(manager.LoadSceneFromFile(m_ScenePath).IsOk());

    EXPECT_EQ(manager.GetRegistry().AllEntities().size(), 1u); // scenes/scene.toml suspended, not live
    EXPECT_EQ(manager.CachedSceneCount(), 1u); // scenes/scene.toml resident-inactive, real path active
    ASSERT_TRUE(manager.CurrentScenePath().has_value());
    EXPECT_EQ(*manager.CurrentScenePath(), m_ScenePath.string());
}

// ─── The shared Registry holds only the active scene -- others are suspended ──

TEST_F(SceneManagerTest, GetRegistry_HoldsOnlyTheActiveScenesEntitiesNotOtherResidentOnes)
{
    WriteValidScene(m_ScenePath, 5.0f);
    auto const pathB = m_Root / "b.toml";
    WriteValidScene(pathB, 1.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    ASSERT_TRUE(manager.LoadScene("scenes/b.toml").IsOk()); // scene.toml suspended, not live

    // Only b.toml's entity is actually live -- a consumer that just
    // enumerates the whole Registry (RenderSystem, AnimationSystem, ...)
    // sees exactly one scene's entities, never a mix of two.
    EXPECT_EQ(manager.GetRegistry().AllEntities().size(), 1u);
    EXPECT_EQ(manager.ActiveEntities().size(), 1u);
    EXPECT_TRUE(manager.EntitiesInScene("scenes/scene.toml").empty()); // suspended, not live

    EXPECT_EQ(manager.CachedSceneCount(), 1u); // still resident, just as a snapshot
}

// ─── UnloadScene ────────────────────────────────────────────────────────────

TEST_F(SceneManagerTest, UnloadScene_DestroysActiveEntitiesAndClearsCurrentPath)
{
    WriteValidScene(m_ScenePath);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    manager.UnloadScene();

    EXPECT_TRUE(manager.ActiveEntities().empty());
    EXPECT_TRUE(manager.GetRegistry().AllEntities().empty()); // nothing else was resident
    EXPECT_FALSE(manager.CurrentScenePath().has_value());
}

TEST_F(SceneManagerTest, UnloadScene_OnlyDestroysTheActiveScenesEntitiesLeavingOthersResident)
{
    WriteValidScene(m_ScenePath, 5.0f);
    auto const pathB = m_Root / "b.toml";
    WriteValidScene(pathB);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    // In-memory mutation that would survive a snapshot restore but not a
    // real reload -- proves scene.toml's suspended snapshot is untouched
    // below, not just that its entity count is unchanged.
    auto entity = manager.ActiveEntities().at(0);
    manager.GetRegistry().GetComponent<Velocity>(entity).Value().get().m_DX = 999.0f;

    ASSERT_TRUE(manager.LoadScene("scenes/b.toml").IsOk()); // b.toml active, scene.toml suspended

    manager.UnloadScene(); // unloads b.toml, the active one

    EXPECT_FALSE(manager.CurrentScenePath().has_value());
    EXPECT_EQ(manager.CachedSceneCount(), 1u); // scene.toml's snapshot untouched

    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk()); // restored from snapshot, not disk
    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 999.0f);
}

// ─── SaveScene ──────────────────────────────────────────────────────────────

TEST_F(SceneManagerTest, SaveScene_SavesOnlyTheActiveScenesEntities)
{
    SceneManager manager{ m_Vfs }; // never loaded anything
    auto entity = manager.GetRegistry().CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(manager.GetRegistry().AddComponent( entity.Value(), Velocity{ 9.0f, 9.0f } ).IsOk());
    // Note: an entity added directly like this has no SceneId, so it's
    // resident but belongs to no scene -- ActiveEntities() (nullopt path)
    // correctly ignores it too; SaveScene below is really exercised by the
    // next test, which is the one that actually has an active scene.

    auto const outPath = m_Root / "saved.toml";
    ASSERT_TRUE(manager.SaveScene(outPath).IsOk()); // no active scene -> saves nothing

    SceneManager reloaded{ m_Vfs };
    ASSERT_TRUE(reloaded.LoadScene("scenes/saved.toml").IsOk());
    EXPECT_TRUE(reloaded.ActiveEntities().empty());
}

TEST_F(SceneManagerTest, SaveScene_DoesNotLeakOtherResidentScenesEntities)
{
    WriteValidScene(m_ScenePath, 5.0f);
    auto const pathB = m_Root / "b.toml";
    WriteValidScene(pathB, 1.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    ASSERT_TRUE(manager.LoadScene("scenes/b.toml").IsOk()); // b.toml active, scene.toml resident-inactive

    auto const outPath = m_Root / "saved.toml";
    ASSERT_TRUE(manager.SaveScene(outPath).IsOk());

    SceneManager reloaded{ m_Vfs };
    ASSERT_TRUE(reloaded.LoadScene("scenes/saved.toml").IsOk());

    auto active = reloaded.ActiveEntities();
    ASSERT_EQ(active.size(), 1u); // only b.toml's entity, not scene.toml's too
    EXPECT_FLOAT_EQ(reloaded.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 1.0f);
}

// ─── RequestLoad / RequestUnload / ApplyPendingTransition ──────────────────

TEST_F(SceneManagerTest, RequestLoad_DoesNotApplyUntilApplyPendingTransition)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };

    manager.RequestLoad("scenes/scene.toml");
    EXPECT_TRUE(manager.HasPendingTransition());
    EXPECT_TRUE(manager.ActiveEntities().empty()); // not applied yet

    ASSERT_TRUE(manager.ApplyPendingTransition().IsOk());
    EXPECT_FALSE(manager.HasPendingTransition());
    EXPECT_EQ(manager.ActiveEntities().size(), 1u);
}

TEST_F(SceneManagerTest, RequestUnload_DoesNotApplyUntilApplyPendingTransition)
{
    WriteValidScene(m_ScenePath);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    manager.RequestUnload();
    EXPECT_TRUE(manager.HasPendingTransition());
    EXPECT_EQ(manager.ActiveEntities().size(), 1u); // not applied yet

    ASSERT_TRUE(manager.ApplyPendingTransition().IsOk());
    EXPECT_FALSE(manager.HasPendingTransition());
    EXPECT_TRUE(manager.ActiveEntities().empty());
    EXPECT_FALSE(manager.CurrentScenePath().has_value());
}

TEST_F(SceneManagerTest, ApplyPendingTransition_NothingPendingIsNoOp)
{
    SceneManager manager{ m_Vfs };
    EXPECT_FALSE(manager.HasPendingTransition());
    EXPECT_TRUE(manager.ApplyPendingTransition().IsOk());
}

TEST_F(SceneManagerTest, RequestLoad_OverwritesAnEarlierPendingRequest)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };

    manager.RequestUnload();
    manager.RequestLoad("scenes/scene.toml"); // supersedes the unload above

    ASSERT_TRUE(manager.ApplyPendingTransition().IsOk());
    EXPECT_EQ(manager.ActiveEntities().size(), 1u);
}

TEST_F(SceneManagerTest, ApplyPendingTransition_FailedLoadStillClearsPendingAndLeavesActiveSceneIntact)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    manager.RequestLoad("scenes/does_not_exist.toml");
    auto result = manager.ApplyPendingTransition();

    EXPECT_FALSE(result.IsOk());
    EXPECT_FALSE(manager.HasPendingTransition()); // not stuck pending forever

    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 5.0f);
    EXPECT_EQ(manager.CachedSceneCount(), 0u); // never suspended
}

// ─── Residency: LoadScene() only reads from disk once per distinct path ───────

TEST_F(SceneManagerTest, LoadScene_AlreadyActiveSceneIsANoOp)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    EXPECT_EQ(manager.CachedSceneCount(), 0u); // never became "resident but inactive" against itself
}

TEST_F(SceneManagerTest, LoadScene_SwapAwayAndBackPreservesLiveStateInsteadOfReloadingFromDisk)
{
    WriteValidScene(m_ScenePath, 5.0f); // on disk: m_DX == 5.0
    auto const otherPath = m_Root / "other.toml";
    WriteValidScene(otherPath, 1.0f);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    // Mutate the live entity to a value that exists only in memory, never
    // written back to scene.toml.
    auto entity = manager.ActiveEntities().at(0);
    manager.GetRegistry().GetComponent<Velocity>(entity).Value().get().m_DX = 999.0f;

    ASSERT_TRUE(manager.LoadScene("scenes/other.toml").IsOk());
    EXPECT_EQ(manager.CachedSceneCount(), 1u); // scene.toml still resident, just not active

    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);

    // 999.0, not 5.0 -- the same entity that was already resident, not a
    // fresh disk read.
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 999.0f);
}

TEST_F(SceneManagerTest, CachedSceneCount_TracksResidentButInactiveScenes)
{
    WriteValidScene(m_ScenePath);
    auto const pathB = m_Root / "b.toml";
    auto const pathC = m_Root / "c.toml";
    WriteValidScene(pathB);
    WriteValidScene(pathC);

    SceneManager manager{ m_Vfs };
    EXPECT_EQ(manager.CachedSceneCount(), 0u);

    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    EXPECT_EQ(manager.CachedSceneCount(), 0u); // only one resident scene, and it's active

    ASSERT_TRUE(manager.LoadScene("scenes/b.toml").IsOk());
    EXPECT_EQ(manager.CachedSceneCount(), 1u); // scene.toml still resident, now inactive

    ASSERT_TRUE(manager.LoadScene("scenes/c.toml").IsOk());
    EXPECT_EQ(manager.CachedSceneCount(), 2u); // b.toml resident-inactive too now

    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    EXPECT_EQ(manager.CachedSceneCount(), 2u); // scene.toml active again, b.toml + c.toml resident-inactive
}

// ─── EvictCachedScene / ClearCache ──────────────────────────────────────────

TEST_F(SceneManagerTest, EvictCachedScene_ForcesTheNextLoadToReadFromDiskAgain)
{
    WriteValidScene(m_ScenePath, 5.0f);
    auto const otherPath = m_Root / "other.toml";
    WriteValidScene(otherPath);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    // In-memory mutation that would survive a residency hit but not a real reload.
    auto entity = manager.ActiveEntities().at(0);
    manager.GetRegistry().GetComponent<Velocity>(entity).Value().get().m_DX = 999.0f;

    ASSERT_TRUE(manager.LoadScene("scenes/other.toml").IsOk());
    ASSERT_EQ(manager.CachedSceneCount(), 1u);

    manager.EvictCachedScene("scenes/scene.toml");
    EXPECT_EQ(manager.CachedSceneCount(), 0u);

    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 5.0f); // disk value, not 999
}

TEST_F(SceneManagerTest, EvictCachedScene_NeverEvictsTheActiveScene)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    manager.EvictCachedScene("scenes/scene.toml"); // it's active, not "cached"

    EXPECT_EQ(manager.ActiveEntities().size(), 1u); // untouched
    ASSERT_TRUE(manager.CurrentScenePath().has_value());
    EXPECT_EQ(*manager.CurrentScenePath(), "scenes/scene.toml");
}

TEST_F(SceneManagerTest, ClearCache_RemovesEveryResidentButInactiveScene)
{
    WriteValidScene(m_ScenePath);
    auto const pathB = m_Root / "b.toml";
    WriteValidScene(pathB);

    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    ASSERT_TRUE(manager.LoadScene("scenes/b.toml").IsOk());
    ASSERT_EQ(manager.CachedSceneCount(), 1u);

    manager.ClearCache();
    EXPECT_EQ(manager.CachedSceneCount(), 0u);
    EXPECT_EQ(manager.ActiveEntities().size(), 1u); // b.toml, still active, untouched
}

TEST_F(SceneManagerTest, UnloadScene_DoesNotLeaveTheUnloadedSceneResident)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());

    // In-memory mutation that would survive residency but not a real reload.
    auto entity = manager.ActiveEntities().at(0);
    manager.GetRegistry().GetComponent<Velocity>(entity).Value().get().m_DX = 999.0f;

    manager.UnloadScene();
    EXPECT_EQ(manager.CachedSceneCount(), 0u); // discarded, not left resident

    // Reloading it has to hit disk again -- the mutation above is gone.
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 1u);
    EXPECT_FLOAT_EQ(manager.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 5.0f);
}

// ─── CreateEntity ───────────────────────────────────────────────────────────

TEST_F(SceneManagerTest, CreateEntity_ActiveSceneTagsEntityAndItAppearsInActiveEntities)
{
    WriteValidScene(m_ScenePath);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk()); // one entity already resident

    auto created = manager.CreateEntity();
    ASSERT_TRUE(created.IsOk());

    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 2u);
    EXPECT_NE(std::find(active.begin(), active.end(), created.Value()), active.end());
}

TEST_F(SceneManagerTest, CreateEntity_SurvivesASaveSceneRoundTrip)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk()); // one entity already resident

    ASSERT_TRUE(manager.CreateEntity().IsOk());
    EXPECT_EQ(manager.ActiveEntities().size(), 2u);

    auto const outPath = m_Root / "created.toml";
    ASSERT_TRUE(manager.SaveScene(outPath).IsOk());

    SceneManager reloaded{ m_Vfs };
    ASSERT_TRUE(reloaded.LoadScene("scenes/created.toml").IsOk());
    EXPECT_EQ(reloaded.ActiveEntities().size(), 2u);
}

TEST_F(SceneManagerTest, CreateEntity_NoActiveSceneReturnsNoActiveSceneError)
{
    SceneManager manager{ m_Vfs }; // never loaded/created anything

    auto result = manager.CreateEntity();
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(SceneError::NoActiveScene));
}

// ─── DuplicateEntity ────────────────────────────────────────────────────────

TEST_F(SceneManagerTest, DuplicateEntity_CopiesSerializableComponentsAndTagsIntoActiveScene)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    auto const source = manager.ActiveEntities().at(0);

    auto duplicated = manager.DuplicateEntity(source);
    ASSERT_TRUE(duplicated.IsOk());
    EXPECT_NE(duplicated.Value(), source);

    auto active = manager.ActiveEntities();
    ASSERT_EQ(active.size(), 2u);
    EXPECT_FLOAT_EQ(
        manager.GetRegistry().GetComponent<Velocity>(duplicated.Value()).Value().get().m_DX, 5.0f);
}

TEST_F(SceneManagerTest, DuplicateEntity_SurvivesASaveSceneRoundTripAsTwoEntities)
{
    WriteValidScene(m_ScenePath, 5.0f);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    auto const source = manager.ActiveEntities().at(0);
    ASSERT_TRUE(manager.DuplicateEntity(source).IsOk());

    auto const outPath = m_Root / "duplicated.toml";
    ASSERT_TRUE(manager.SaveScene(outPath).IsOk());

    SceneManager reloaded{ m_Vfs };
    ASSERT_TRUE(reloaded.LoadScene("scenes/duplicated.toml").IsOk());

    auto active = reloaded.ActiveEntities();
    ASSERT_EQ(active.size(), 2u);
    EXPECT_FLOAT_EQ(reloaded.GetRegistry().GetComponent<Velocity>(active[0]).Value().get().m_DX, 5.0f);
    EXPECT_FLOAT_EQ(reloaded.GetRegistry().GetComponent<Velocity>(active[1]).Value().get().m_DX, 5.0f);
}

TEST_F(SceneManagerTest, DuplicateEntity_NoActiveSceneReturnsNoActiveSceneError)
{
    WriteValidScene(m_ScenePath);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    auto const source = manager.ActiveEntities().at(0);

    manager.UnloadScene(); // nothing active anymore, but source's handle is now stale too -- see below

    auto result = manager.DuplicateEntity(source);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(SceneError::NoActiveScene));
}

TEST_F(SceneManagerTest, DuplicateEntity_DeadEntityReturnsEntityIsNotAliveError)
{
    WriteValidScene(m_ScenePath);
    SceneManager manager{ m_Vfs };
    ASSERT_TRUE(manager.LoadScene("scenes/scene.toml").IsOk());
    auto const source = manager.ActiveEntities().at(0);
    ASSERT_TRUE(manager.GetRegistry().DestroyEntity(source).IsOk());

    auto result = manager.DuplicateEntity(source);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(EcsError::EntityIsNotAlive));
}

}
