#pragma once

#include <optional>
#include <vector>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Assets/ScenePrefab.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include "IdContext.hpp"

namespace asge::game::scene
{

/**
 * @brief Serializes an ecs::Registry's entities to and from a TOML scene file.
 *
 * Save() writes one `[[entity]]` table per alive entity; Load() and
 * LoadFromFile() read such a file back into a registry.
 */
class SceneSerializer
{
    filesystem::VirtualFileSystem const& m_Vfs; // VFS scene asset paths resolve against
public:
    explicit SceneSerializer( filesystem::VirtualFileSystem const& inVfs ) noexcept
        : m_Vfs( inVfs )
    {}

    SceneSerializer( SceneSerializer const& ) = default;
    SceneSerializer( SceneSerializer&& ) = default;
    SceneSerializer& operator=( SceneSerializer const& ) = default;
    SceneSerializer& operator=( SceneSerializer&& ) = default;

    /**
     * @brief Destructor.
     *
     * Defaulted; members clean up after themselves.
     */
    ~SceneSerializer() = default;

    /**
     * @brief Writes every currently-alive entity in inRegistry to inPath as TOML.
     *
     * One `[[entity]]` array-of-tables entry per entity, with a subtable for
     * each serializable component it currently has. A non-null outCtx receives
     * each entity's index in the file (m_Ids). Returns the underlying
     * filesystem::WriteText error if the write fails.
     */
    BoolResult Save(
        ecs::Registry const& inRegistry, filesystem::Path const& inPath, 
        SaveContext* outCtx = nullptr 
    ) const noexcept;

    /**
     * @brief Writes inRoot and its descendants to inPath as a prefab file.
     *
     * Same format as Save(), but only the subtree under inRoot, with the root
     * first and saved as Id 0. Its link to a parent outside the subtree is
     * dropped, so the file always has exactly one root. inRoot must be alive.
     * A non-null outCtx receives each entity's file id.
     */
    BoolResult SavePrefab(
        ecs::Registry const& inRegistry, ecs::Entity inRoot, filesystem::Path const& inPath,
        SaveContext* outCtx = nullptr
    ) const noexcept;

    /**
     * @brief Populates dstRegistry from a scene file previously written by Save().
     *
     * Creates one entity per `[[entity]]` block and re-adds each serializable
     * component via its Serializer. A non-null outCtx receives each file index's
     * new entity on success. On failure only the entities this call created are
     * rolled back; existing ones are never touched.
     */
    BoolResult Load(
        ecs::Registry& dstRegistry, str::String const& inVirtualPath, 
        LoadContext* outCtx = nullptr 
    ) const noexcept;

    /**
     * @brief Same as Load(), but for a scene file addressed by a real filesystem path.
     *
     * The path is used as is instead of being resolved through the VFS, with
     * the same partial-load rollback guarantee as Load().
     */
    BoolResult LoadFromFile(
        ecs::Registry& dstRegistry, filesystem::Path const& inPath, 
        LoadContext* outCtx = nullptr 
    ) const noexcept;

    /**
     * @brief Spawns a fresh copy of a prefab (root plus descendants) into outRegistry.
     *
     * Creates new entities and components each call, so instances are
     * independent of the prefab and of each other. A given inTransform replaces
     * the root's Transform. Returns the root entity. On failure nothing is left
     * behind in outRegistry.
     */
    Result<ecs::Entity> Instantiate(
        ecs::Registry& outRegistry, game::asset::ScenePrefab const& inPrefab,
        std::optional<game::components::Transform> inTransform = std::nullopt
    ) const noexcept;

private:
    /**
     * @brief Shared body of Load() and LoadFromFile().
     *
     * Runs once the scene path has been resolved to a real path.
     */
    BoolResult LoadResolved(
        ecs::Registry& dstRegistry, filesystem::Path const& inPath,  LoadContext* outCtx 
    ) const noexcept;

    /**
     * @brief Shared body of Save() and SavePrefab().
     *
     * Gives inEntities the ids 0..n-1 in order and writes each one's
     * serializable components. Links to entities outside inEntities are
     * saved as -1.
     */
    BoolResult WriteEntities(
        ecs::Registry const& inRegistry, std::vector<ecs::Entity> const& inEntities,
        filesystem::Path const& inPath, SaveContext* outCtx
    ) const noexcept;

    /**
     * @brief Shared body of every load: creates the entities of a parsed scene table.
     *
     * Reads each `[[entity]]` of inTv, adds its components and rebuilds the
     * Hierarchy links. A non-null outCtx receives the file id to entity map.
     * On failure only the entities this call created are rolled back.
     */
    BoolResult Populate(
        ecs::Registry& outRegistry, formats::toml::TOMLTableView inTv, LoadContext* outCtx
    ) const noexcept;
};

}