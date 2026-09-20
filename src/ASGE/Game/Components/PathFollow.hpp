#pragma once

#include <ASGE/Core/Math/Geometry/CatmullRomSpline.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief An entity's authored path (as Catmull-Rom waypoints) plus its
 *        runtime progress along it.
 *
 * m_Waypoints/m_Speed/m_Loop/m_Resolution are what a scene file describes;
 * asset::AssetManager::ResolveAssets (via asset::Resolver<PathFollow>)
 * builds m_Path — a math::CatmullRomSpline through m_Waypoints, sampled at
 * m_Resolution arc-length steps per segment — the first time it sees a
 * non-empty m_Waypoints with no path built yet, same deferred-resolve shape
 * as Sprite::m_Texture/Animation::m_Clip. m_Traveled/m_Finished are plain
 * runtime state for whatever advances an entity along m_Path each frame.
 */
struct PathFollow
{
    std::vector<math::Float2> m_Waypoints;      // Authored control points the path passes through, in order
    float                     m_Speed{ 1.0f };  // Units travelled per second along m_Path
    bool                      m_Loop{ false };  // Whether reaching the end wraps back to the start instead of stopping
    std::size_t               m_Resolution{32}; // Arc-length samples per segment, passed straight to CatmullRomSpline's constructor

    // Runtime-only parameters — never round-tripped through TOML, always reset to these defaults by FromToml
    math::CatmullRomSpline m_Path;            // Built from m_Waypoints by asset::Resolver<PathFollow> once resolved; no segments until then
    float                  m_Traveled{0.0f};  // Distance travelled along m_Path so far
    bool                   m_Finished{false}; // Whether a non-looping path has reached its end
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_Waypoints/m_Speed/m_Loop/m_Resolution only — see
 *        AudioSource's Serializer doc comment for why a scene file
 *        describes what an entity's path is, not where a previous run left
 *        it. FromToml leaves m_Path/m_Traveled/m_Finished at PathFollow's
 *        in-code defaults, resolved later by asset::Resolver<PathFollow>.
 */
template<>
struct Serializer<components::PathFollow>
{
    static constexpr str::StringView kTableName = "PathFollow";

    using T = components::PathFollow;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}
