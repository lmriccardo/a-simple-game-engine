#pragma once

#include <ASGE/Core/Math/Interpolation/CatmullRomSpline.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief A route that child entities carrying a PathFollow walk along.
 *
 * Waypoints are local to this entity's Transform, so moving, rotating or
 * scaling the route entity moves the whole path. asset::Resolver<Path>
 * builds m_Path from m_Waypoints once, same deferred-resolve shape as
 * Sprite::m_Texture; call RebuildPath after mutating the waypoints later.
 */
struct Path
{
    // Serializable values
    std::vector<math::Float2> m_Waypoints  {};        // Control points the path passes through, in order, local to this entity
    std::size_t               m_Resolution { 32 };    // Arc-length samples per segment, passed to CatmullRomSpline
    bool                      m_Loop       { false }; // Whether a follower reaching the end wraps to the start instead of stopping

    // Runtime values only
    math::CatmullRomSpline m_Path; // Built from m_Waypoints by asset::Resolver<Path>; no segments until then
};

/**
 * @brief Rebuilds inPath.m_Path from its current m_Waypoints/m_Resolution.
 *        Call it after mutating either field, since the resolver only
 *        builds the spline once.
 */
void RebuildPath( Path& inPath ) noexcept;

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_Waypoints/m_Loop/m_Resolution only; FromToml leaves
 *        m_Path at its default, built later by asset::Resolver<Path>.
 */
template<>
struct Serializer<components::Path>
{
    static constexpr str::StringView kTableName = "Path";

    using T = components::Path;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}