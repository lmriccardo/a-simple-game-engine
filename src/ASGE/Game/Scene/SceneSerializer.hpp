#pragma once

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::scene
{

/**
 * @brief Serializes an ecs::Registry's entities to a TOML scene file.
 */
class SceneSerializer
{
    filesystem::VirtualFileSystem const& m_Vfs; // VFS scene asset paths resolve against; not yet used by Save()
public:
    /** @brief Binds this serializer to the VFS scene asset paths are resolved through. */
    explicit SceneSerializer( filesystem::VirtualFileSystem const& inVfs ) noexcept
    : m_Vfs( inVfs )
    {}

    SceneSerializer( SceneSerializer const& ) = default;
    SceneSerializer( SceneSerializer&& ) = default;
    SceneSerializer& operator=( SceneSerializer const& ) = default;
    SceneSerializer& operator=( SceneSerializer&& ) = default;

    ~SceneSerializer() = default;

    /**
     * @brief Writes every currently-alive entity in inRegistry to inPath as
     *        TOML: one `[[entity]]` array-of-tables entry per entity, with
     *        a subtable for each serializable component it currently has.
     * @return Ok on success, or the underlying filesystem::WriteText error.
     */
    BoolResult Save( ecs::Registry const& inRegistry, filesystem::Path const& inPath ) const noexcept;

    /**
     * @brief Populates dstRegistry from a scene file previously written by
     *        Save(): one fresh CreateEntity() per `[[entity]]` block, with
     *        every serializable component present re-added via its Serializer.
     * @return Ok on success. On any failure, only the entities this call
     *         itself created are rolled back — anything already in
     *         dstRegistry before the call is never touched, win or lose.
     */
    BoolResult Load( ecs::Registry& dstRegistry, str::String const& inVirtualPath ) const noexcept;
};

}