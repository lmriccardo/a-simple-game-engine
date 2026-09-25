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
 * @brief Owns one Registry holding only the active scene's entities, and
 *        switches which scene is "active" without reloading one it has
 *        already visited.
 *
 * Every other visited scene stays resident as an in-memory snapshot (see
 * m_Snapshots) rather than as live entities in the shared Registry, so
 * exactly one scene's entities are ever enumerable at a time and a switch
 * back to a snapshotted scene is a fast in-memory restore, not a disk read.
 */
class SceneManager
{
    filesystem::VirtualFileSystem const& m_Vfs;
    SceneSerializer m_Serializer{ m_Vfs };

    ecs::Registry m_Registry; // only the active scene's entities, tagged by SceneId
    std::optional<str::String> m_CurrentScenePath; // nullopt: nothing active

    // Every other visited scene's entities, keyed by SceneId path, held as
    // its own scratch Registry instead of live in m_Registry -- see
    // SuspendScene()/RestoreScene().
    std::unordered_map<str::String, ecs::Registry> m_Snapshots;

    // A transition requested via RequestLoad()/RequestUnload(), applied on
    // the next ApplyPendingTransition() call. At most one is pending at a
    // time — a later request before that call overwrites an earlier one.
    enum class PendingKind { None, Load, Unload };
    PendingKind m_PendingKind{ PendingKind::None };
    str::String m_PendingPath;

    // Copies every serializable component inSrcEntity has, in inSrc, onto inDstEntity in inDst
    void CopyEntityComponents(
        ecs::Registry const& inSrc, ecs::Entity inSrcEntity,
        ecs::Registry& inDst, ecs::Entity inDstEntity ) const noexcept;

    // Moves inSceneId's live entities out of m_Registry into a new
    // m_Snapshots entry, via component-copy into a scratch Registry followed
    // by destroying the originals. inSceneId must currently be live (i.e. be
    // m_CurrentScenePath) -- callers are responsible for that.
    void SuspendScene( str::String const& inSceneId ) noexcept;

    // Copies inSnapshot's entities into m_Registry, tagging each with
    // inSceneId's SceneId -- SuspendScene()'s counterpart. Does not touch
    // m_Snapshots or m_CurrentScenePath; callers manage both.
    void RestoreScene( str::String const& inSceneId, ecs::Registry const& inSnapshot ) noexcept;

    // Shared body of LoadScene()/LoadSceneFromFile(): handles the
    // already-active/already-snapshotted short-circuits, SceneId tagging,
    // and suspending the outgoing scene once inLoad has actually populated
    // the Registry, however it got there.
    BoolResult LoadSceneCommon(
        str::String const& inSceneId,
        std::function<BoolResult( ecs::Registry& )> const& inLoad ) noexcept;

public:
    explicit SceneManager( filesystem::VirtualFileSystem const& inVfs ) noexcept
        : m_Vfs( inVfs )
    {}

    /**
     * @brief Makes inVirtualPath the active scene — instantly if already
     *        resident, otherwise loaded from disk and tagged. No-op if
     *        already active.
     * @return Ok on success; a failed disk load disturbs nothing resident.
     */
    BoolResult LoadScene( str::String const& inVirtualPath ) noexcept;

    /**
     * @brief LoadScene()'s counterpart for a scene file addressed by a real
     *        filesystem path rather than a VFS one — symmetric with SaveScene().
     *        The SceneId its entities are tagged with defaults to inPath's string form.
     * @return Ok on success; a failed disk load disturbs nothing resident.
     */
    BoolResult LoadSceneFromFile( filesystem::Path const& inPath ) noexcept;

    /** @brief Destroys every entity belonging to the active scene, leaving nothing active. */
    void UnloadScene() noexcept;

    /** @brief Saves only the active scene's entities to inPath, not every resident scene. */
    BoolResult SaveScene( filesystem::Path const& inPath ) const noexcept;

    /**
     * @brief Drops inVirtualPath's snapshot so the next LoadScene() for it
     *        rereads from disk. No-op if it isn't snapshotted, including if
     *        it's the active scene — use UnloadScene() for that.
     */
    void EvictCachedScene( str::String const& inVirtualPath ) noexcept;

    /** @brief Drops every snapshotted scene, leaving the active one untouched — see EvictCachedScene(). */
    void ClearCache() noexcept;

    /** @brief How many distinct scenes are snapshotted besides the active one. */
    [[nodiscard]] std::size_t CachedSceneCount() const noexcept;

    /** @brief Queues a scene load for the next ApplyPendingTransition() call, overwriting any pending one. */
    void RequestLoad( str::String const& inVirtualPath ) noexcept;

    /** @brief Queues an unload for the next ApplyPendingTransition() call. */
    void RequestUnload() noexcept;

    /** @brief True if a RequestLoad()/RequestUnload() transition is still waiting to apply. */
    [[nodiscard]] bool HasPendingTransition() const noexcept;

    /** @brief Applies the queued transition, if any — a no-op returning Ok when nothing is pending. */
    BoolResult ApplyPendingTransition() noexcept;

    /**
     * @brief Every entity tagged with inVirtualPath's SceneId in the live
     *        Registry. Empty for a merely snapshotted (resident-but-inactive)
     *        scene, since it has no live entities to tag — see ActiveEntities().
     */
    [[nodiscard]] std::vector<ecs::Entity> EntitiesInScene( str::String const& inVirtualPath ) const noexcept;

    /** @brief EntitiesInScene() for the active scene, or empty if none is active. */
    [[nodiscard]] std::vector<ecs::Entity> ActiveEntities() const noexcept;

    /**
     * @brief Creates a fresh entity already tagged with the active scene's
     *        SceneId, so it shows up in ActiveEntities() and SaveScene()
     *        without the caller having to tag it by hand.
     * @return The new entity, or an error if no scene is active.
     */
    [[nodiscard]] Result<ecs::Entity> CreateEntity() noexcept;

    /**
     * @brief Creates a new entity carrying a copy of every serializable
     *        component inEntity has, tagged into the active scene.
     * @return The new entity, or an error if no scene is active or
     *         inEntity isn't currently alive.
     */
    [[nodiscard]] Result<ecs::Entity> DuplicateEntity( ecs::Entity inEntity ) noexcept;

    /** @brief The Registry backing only the active scene — other resident scenes are snapshotted, not here. */
    [[nodiscard]] ecs::Registry& GetRegistry() noexcept { return m_Registry; }

    /** @brief Read-only access to the active scene's Registry — see GetRegistry(). */
    [[nodiscard]] ecs::Registry const& GetRegistry() const noexcept { return m_Registry; }

    /** @brief Virtual path the active scene was loaded from, or nullopt if none. */
    [[nodiscard]] std::optional<str::String> const& CurrentScenePath() const noexcept 
    { return m_CurrentScenePath; }
};

}
