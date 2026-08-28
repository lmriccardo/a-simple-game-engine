#pragma once

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::scene
{

/**
 * @brief Serializes an ecs::Registry's entities to a TOML scene file.
 *
 * For each entity, walks components::SerializableComponents and asks the
 * registry which of those types the entity currently has, delegating the
 * actual per-field TOML shape to that type's Serializer<T> specialization
 * (see Components/Serialize.hpp) — this class only orchestrates the walk.
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

    BoolResult Load( ecs::Registry& dstRegistry, str::String const& inVirtualPath ) const noexcept;
};

}