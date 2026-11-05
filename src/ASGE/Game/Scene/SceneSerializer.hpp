#pragma once

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
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
    filesystem::VirtualFileSystem const& m_Vfs; // VFS scene asset paths resolve against; not yet used by Save()
public:
    /**
     * @brief Binds this serializer to the VFS scene asset paths are resolved through.
     *
     * inVfs is held by reference and must outlive the serializer.
     */
    explicit SceneSerializer( filesystem::VirtualFileSystem const& inVfs ) noexcept
    : m_Vfs( inVfs )
    {}

    /**
     * @brief Copy constructor.
     *
     * Defaulted; copies each member.
     */
    SceneSerializer( SceneSerializer const& ) = default;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    SceneSerializer( SceneSerializer&& ) = default;

    /**
     * @brief Copy assignment.
     *
     * Defaulted; assigns each member.
     */
    SceneSerializer& operator=( SceneSerializer const& ) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
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
        ecs::Registry const& inRegistry, filesystem::Path const& inPath, SaveContext* outCtx = nullptr ) const noexcept;

    /**
     * @brief Populates dstRegistry from a scene file previously written by Save().
     *
     * Creates one entity per `[[entity]]` block and re-adds each serializable
     * component via its Serializer. A non-null outCtx receives each file index's
     * new entity on success. On failure only the entities this call created are
     * rolled back; existing ones are never touched.
     */
    BoolResult Load(
        ecs::Registry& dstRegistry, str::String const& inVirtualPath, LoadContext* outCtx = nullptr ) const noexcept;

    /**
     * @brief Same as Load(), but for a scene file addressed by a real filesystem path.
     *
     * The path is used as is instead of being resolved through the VFS, with
     * the same partial-load rollback guarantee as Load().
     */
    BoolResult LoadFromFile(
        ecs::Registry& dstRegistry, filesystem::Path const& inPath, LoadContext* outCtx = nullptr ) const noexcept;

private:
    /**
     * @brief Shared body of Load() and LoadFromFile().
     *
     * Runs once the scene path has been resolved to a real path.
     */
    BoolResult LoadResolved(
        ecs::Registry& dstRegistry, filesystem::Path const& inPath, LoadContext* outCtx ) const noexcept;
};

}