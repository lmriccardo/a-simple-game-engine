#pragma once

#include <ASGE/Core/Math/Interpolation/CatmullRomSpline.hpp>
#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Moves its entity along the Path of its parent entity.
 *
 * The follower must be a child (via Hierarchy) of an entity holding a Path;
 * PathFollowingSystem samples that Path and writes the point, plus the
 * offset, into this entity's local Transform. No entity reference is stored.
 *
 * Every frame it moves, PathFollowingSystem also writes the path's unit
 * tangent into m_Heading, whether or not m_AlignRotation is set, so a game
 * can pick a facing (e.g. an animation clip) without re-sampling the Path.
 * It is expressed in the same space as the follower's local Transform (the
 * parent Path entity's space), so a rotated or scaled Path entity needs no
 * manual correction. A degenerate segment keeps the last valid value.
 * m_Heading is never serialized.
 */
struct PathFollow
{
    // Serializable values
    float m_Speed   { 1.0f }; // Units travelled per second along the parent's Path
    float m_OffsetH { 0.0f }; // Horizontal offset added to the sampled point
    float m_OffsetV { 0.0f }; // Vertical offset added to the sampled point
    bool  m_AlignRotation { true }; // Whether the local rotation is forced to face the path tangent

    // Runtime values only
    float m_Traveled { 0.0f };  // Distance travelled along the Path so far
    bool  m_Finished { false }; // Whether a non-looping Path has been fully walked
    math::Float2 m_Heading { 1.0f, 0.0f }; // Unit path tangent in the parent's space, updated every frame it moves; +X until first set
    bool  m_WarnedNoPath { false }; // Whether the "no Path parent" warning was already logged for this follower
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_Speed/m_OffsetH/m_OffsetV/m_AlignRotation only; FromToml leaves
 *        m_Traveled/m_Finished/m_Heading at their defaults.
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
