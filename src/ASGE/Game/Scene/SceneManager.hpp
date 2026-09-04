#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include "SceneId.hpp"
#include "SceneSerializer.hpp"

namespace asge::game::scene
{

/**
 * @brief Owns one Registry shared by every scene it knows about, and
 *        changes which one is "active" without reloading a scene it has
 *        already visited.
 */
class SceneManager
{
    filesystem::VirtualFileSystem const& m_Vfs;
    SceneSerializer m_Serializer{ m_Vfs };

    ecs::Registry m_Registry; // every resident scene's entities, tagged by SceneId
    std::optional<str::String> m_CurrentScenePath; // nullopt: nothing active

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

    /** @brief Destroys every entity belonging to the active scene, leaving nothing active. */
    void UnloadScene() noexcept;

    /** @brief Saves only the active scene's entities to inPath, not every resident scene. */
    BoolResult SaveScene( filesystem::Path const& inPath ) const noexcept;

    /**
     * @brief Frees inVirtualPath's entities so the next LoadScene() for it
     *        rereads from disk. No-op if it isn't resident, including if
     *        it's the active scene — use UnloadScene() for that.
     */
    void EvictCachedScene( str::String const& inVirtualPath ) noexcept;

    /** @brief Evicts every resident scene except the active one — see EvictCachedScene(). */
    void ClearCache() noexcept;

    /** @brief How many distinct scenes are resident besides the active one. */
    [[nodiscard]] std::size_t CachedSceneCount() const noexcept;

    /** @brief Queues a scene load for the next ApplyPendingTransition() call, overwriting any pending one. */
    void RequestLoad( str::String const& inVirtualPath ) noexcept;

    /** @brief Queues an unload for the next ApplyPendingTransition() call. */
    void RequestUnload() noexcept;

    /** @brief True if a RequestLoad()/RequestUnload() transition is still waiting to apply. */
    [[nodiscard]] bool HasPendingTransition() const noexcept;

    /** @brief Applies the queued transition, if any — a no-op returning Ok when nothing is pending. */
    BoolResult ApplyPendingTransition() noexcept;

    /** @brief Every entity tagged with inVirtualPath's SceneId, active or not. */
    [[nodiscard]] std::vector<ecs::Entity> EntitiesInScene( str::String const& inVirtualPath ) const noexcept;

    /** @brief EntitiesInScene() for the active scene, or empty if none is active. */
    [[nodiscard]] std::vector<ecs::Entity> ActiveEntities() const noexcept;

    /** @brief The Registry backing every resident scene, not just the active one — see ActiveEntities(). */
    [[nodiscard]] ecs::Registry& GetRegistry() noexcept { return m_Registry; }
    
    /** @brief Read-only access to the shared Registry — see GetRegistry(). */
    [[nodiscard]] ecs::Registry const& GetRegistry() const noexcept { return m_Registry; }

    /** @brief Virtual path the active scene was loaded from, or nullopt if none. */
    [[nodiscard]] std::optional<str::String> const& CurrentScenePath() const noexcept 
    { return m_CurrentScenePath; }
};

}
