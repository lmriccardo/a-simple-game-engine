#pragma once

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::scene
{

class SceneSerializer
{
    filesystem::VirtualFileSystem const& m_Vfs;
public:
    explicit SceneSerializer( filesystem::VirtualFileSystem const& inVfs ) noexcept
    : m_Vfs( inVfs )
    {}

    SceneSerializer( SceneSerializer const& ) = default;
    SceneSerializer( SceneSerializer&& ) = default;
    SceneSerializer& operator=( SceneSerializer const& ) = default;
    SceneSerializer& operator=( SceneSerializer&& ) = default;

    ~SceneSerializer() = default;

    BoolResult Save( ecs::Registry const& inRegistry, filesystem::Path const& inPath ) const noexcept;
};

}