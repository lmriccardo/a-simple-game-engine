#pragma once

#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Moves its entity along the Path of its parent entity.
 *
 * The follower must be a child (via Hierarchy) of an entity holding a Path;
 * PathFollowingSystem samples that Path and writes the point, plus the
 * offset, into this entity's local Transform. No entity reference is stored.
 */
struct PathFollow
{
    // Serializable values
    float m_Speed   { 1.0f }; // Units travelled per second along the parent's Path
    float m_OffsetH { 0.0f }; // Horizontal offset added to the sampled point
    float m_OffsetV { 0.0f }; // Vertical offset added to the sampled point

    // Runtime values only
    float m_Traveled { 0.0f };  // Distance travelled along the Path so far
    bool  m_Finished { false }; // Whether a non-looping Path has been fully walked
};

}

namespace asge::game::scene
{

/**
 * @brief Round-trips m_Speed/m_OffsetH/m_OffsetV only; FromToml leaves
 *        m_Traveled/m_Finished at their defaults.
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
