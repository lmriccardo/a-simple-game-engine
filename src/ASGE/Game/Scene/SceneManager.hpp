#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include "SceneId.hpp"
#include "SceneSerializer.hpp"

namespace asge::game::scene
{

/**
 * @brief Owns one Registry holding only the active scene's entities.
 *
 * Switches which scene is "active" without reloading one it has already
 * visited.
 *
 * Every other visited scene stays resident as an in-memory snapshot (see
 * m_Snapshots) rather than as live entities in the shared Registry, so
 * exactly one scene's entities are ever enumerable at a time and a switch
 * back to a snapshotted scene is a fast in-memory restore, not a disk read.
 */
class SceneManager
{
    filesystem::VirtualFileSystem const& m_Vfs;      // Resolves scene virtual paths; not owned
    SceneSerializer m_Serializer{ m_Vfs };           // Reads and writes scene files

    ecs::Registry m_Registry; // only the active scene's entities, tagged by SceneId
    std::optional<str::String> m_CurrentScenePath; // nullopt: nothing active

    // Every other visited scene's entities, keyed by SceneId path, held as
    // its own scratch Registry instead of live in m_Registry -- see
    // SuspendScene()/RestoreScene().
    std::unordered_map<str::String, ecs::Registry> m_Snapshots; // Suspended scenes by SceneId path
    // For each snapshotted scene, which snapshot entity each of its formerly
    // live entities became -- see TakeRestoredEntities().
    std::unordered_map<str::String, std::unordered_map<ecs::Entity, ecs::Entity>> m_SuspendedHandles; // Old -> snapshot handle, per scene
    // The last LoadScene()'s snapshot restore: each entity's handle before the
    // scene was suspended -> its handle now.
    std::unordered_map<ecs::Entity, ecs::Entity> m_RestoredEntities; // Old -> new handle of the last restore

    // A transition requested via RequestLoad()/RequestUnload(), applied on
    // the next ApplyPendingTransition() call. At most one is pending at a
    // time — a later request before that call overwrites an earlier one.
    enum class PendingKind { None, Load, Unload };
    PendingKind m_PendingKind{ PendingKind::None }; // Which transition is queued, if any
    str::String m_PendingPath;                       // Scene to load when m_PendingKind is Load

    /**
     * @brief Copies every serializable component of an entity onto another.
     *
     * Reads inSrcEntity in inSrc and writes onto inDstEntity in inDst, which
     * may be different registries.
     */
    void CopyEntityComponents(
        ecs::Registry const& inSrc, ecs::Entity inSrcEntity,
        ecs::Registry& inDst, ecs::Entity inDstEntity ) const noexcept;

    /**
     * @brief Creates one entity in inDst per inEntities element.
     *
     * Copies its components and rewrites its Hierarchy links to the copies (a
     * link to an entity not in inEntities becomes Null). Returns each source
     * entity's copy.
     */
    Result<std::unordered_map<ecs::Entity, ecs::Entity>> CopyEntities(
        ecs::Registry const& inSrc, std::vector<ecs::Entity> const& inEntities,
        ecs::Registry& inDst ) const noexcept;

    /**
     * @brief Moves inSceneId's live entities out of m_Registry into a snapshot.
     *
     * Copies the components into a scratch Registry stored in m_Snapshots, then
     * destroys the originals. inSceneId must currently be live (i.e. be
     * m_CurrentScenePath) -- callers are responsible for that.
     */
    void SuspendScene( str::String const& inSceneId ) noexcept;

    /**
     * @brief Copies inSnapshot's entities into m_Registry, tagged with inSceneId.
     *
     * SuspendScene()'s counterpart. Does not touch m_Snapshots or
     * m_CurrentScenePath; callers manage both.
     */
    void RestoreScene( str::String const& inSceneId, ecs::Registry const& inSnapshot ) noexcept;

    /**
     * @brief Shared body of LoadScene() and LoadSceneFromFile().
     *
     * Handles the already-active and already-snapshotted short-circuits,
     * SceneId tagging, and suspending the outgoing scene once inLoad has
     * actually populated the Registry, however it got there.
     */
    BoolResult LoadSceneCommon(
        str::String const& inSceneId,
        std::function<BoolResult( ecs::Registry& )> const& inLoad ) noexcept;

public:
    /**
     * @brief Creates a manager with no active scene.
     *
     * inVfs is held by reference and must outlive the manager.
     */
    explicit SceneManager( filesystem::VirtualFileSystem const& inVfs ) noexcept
        : m_Vfs( inVfs )
    {}

    /**
     * @brief Makes inVirtualPath the active scene.
     *
     * Instant if already resident, otherwise loaded from disk and tagged; a
     * no-op if already active. A non-null outCtx is cleared, then filled (file
     * index -> entity) when this call actually read the scene from disk, and
     * stays empty for a scene that was already active or restored from a
     * snapshot. A failed disk load disturbs nothing resident.
     */
    BoolResult LoadScene( str::String const& inVirtualPath, LoadContext* outCtx = nullptr ) noexcept;

    /**
     * @brief LoadScene()'s counterpart for a scene file addressed by a real path.
     *
     * Symmetric with SaveScene(). The SceneId its entities are tagged with
     * defaults to inPath's string form. A failed disk load disturbs nothing
     * resident.
     */
    BoolResult LoadSceneFromFile( filesystem::Path const& inPath, LoadContext* outCtx = nullptr ) noexcept;

    /**
     * @brief Destroys every entity belonging to the active scene.
     *
     * Leaves nothing active; snapshotted scenes are kept.
     */
    void UnloadScene() noexcept;

    /**
     * @brief Makes inNewVirtualPath the active scene's identity going forward.
     *
     * Retags every currently-active entity's SceneId to match, so
     * ActiveEntities()/SaveScene() keep seeing them under the new path.
     * Touches neither disk nor the Registry's actual entity set.
     *
     * Used for "Save As" (retags a populated active scene under a new
     * path) and, paired with a prior UnloadScene(), for starting a scene
     * from scratch -- nothing active left to retag, so this just
     * establishes the identity newly-created entities get tagged under.
     */
    void RenameActiveScene( str::String const& inNewVirtualPath ) noexcept;

    /**
     * @brief Saves only the active scene's entities to inPath, not every resident scene.
     *
     * A non-null outCtx receives each live entity's index in the saved file.
     */
    BoolResult SaveScene( filesystem::Path const& inPath, SaveContext* outCtx = nullptr ) const noexcept;

    /**
     * @brief Returns the old -> new entity handles of the last snapshot restore.
     *
     * After a LoadScene() that restored a snapshot it maps each entity's
     * handle before that scene was suspended to its handle now (suspending
     * and restoring copy entities, so handles change). Empty otherwise.
     * Returns it and clears it.
     */
    [[nodiscard]] std::unordered_map<ecs::Entity, ecs::Entity> TakeRestoredEntities() noexcept;

    /**
     * @brief Drops inVirtualPath's snapshot so the next LoadScene() for it rereads from disk.
     *
     * No-op if it isn't snapshotted, including if it's the active scene —
     * use UnloadScene() for that.
     */
    void EvictCachedScene( str::String const& inVirtualPath ) noexcept;

    /**
     * @brief Drops every snapshotted scene, leaving the active one untouched.
     *
     * See EvictCachedScene().
     */
    void ClearCache() noexcept;

    /**
     * @brief How many distinct scenes are snapshotted besides the active one.
     *
     * The active scene is not counted.
     */
    [[nodiscard]] std::size_t CachedSceneCount() const noexcept;

    /**
     * @brief Queues a scene load for the next ApplyPendingTransition() call.
     *
     * Overwrites any pending request; nothing is loaded until then.
     */
    void RequestLoad( str::String const& inVirtualPath ) noexcept;

    /**
     * @brief Queues an unload for the next ApplyPendingTransition() call.
     *
     * Overwrites any pending request; nothing is unloaded until then.
     */
    void RequestUnload() noexcept;

    /**
     * @brief True if a RequestLoad()/RequestUnload() transition is still waiting to apply.
     *
     * Cleared by ApplyPendingTransition().
     */
    [[nodiscard]] bool HasPendingTransition() const noexcept;

    /**
     * @brief Applies the queued transition, if any.
     *
     * A no-op returning Ok when nothing is pending.
     */
    BoolResult ApplyPendingTransition() noexcept;

    /**
     * @brief Every entity tagged with inVirtualPath's SceneId in the live Registry.
     *
     * Empty for a merely snapshotted (resident-but-inactive) scene, since it
     * has no live entities to tag — see ActiveEntities().
     */
    [[nodiscard]] std::vector<ecs::Entity> EntitiesInScene( str::String const& inVirtualPath ) const noexcept;

    /**
     * @brief EntitiesInScene() for the active scene.
     *
     * Empty if none is active.
     */
    [[nodiscard]] std::vector<ecs::Entity> ActiveEntities() const noexcept;

    /**
     * @brief Creates a fresh entity already tagged with the active scene's SceneId.
     *
     * It shows up in ActiveEntities() and SaveScene() without the caller
     * having to tag it by hand. Returns an error if no scene is active.
     */
    [[nodiscard]] Result<ecs::Entity> CreateEntity() noexcept;

    /**
     * @brief Creates a new entity carrying a copy of every serializable component of inEntity.
     *
     * The copy is tagged into the active scene. Returns an error if no scene
     * is active or inEntity isn't currently alive.
     */
    [[nodiscard]] Result<ecs::Entity> DuplicateEntity( ecs::Entity inEntity ) noexcept;

    /**
     * @brief The Registry backing only the active scene.
     *
     * Other resident scenes are snapshotted, not here.
     */
    [[nodiscard]] ecs::Registry& GetRegistry() noexcept { return m_Registry; }

    /**
     * @brief Read-only access to the active scene's Registry.
     *
     * See GetRegistry().
     */
    [[nodiscard]] ecs::Registry const& GetRegistry() const noexcept { return m_Registry; }

    /**
     * @brief Virtual path the active scene was loaded from.
     *
     * Nullopt if none is active.
     */
    [[nodiscard]] std::optional<str::String> const& CurrentScenePath() const noexcept 
    { return m_CurrentScenePath; }
};

}
