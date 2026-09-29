#pragma once

#include <ASGE/Game/Scene/Serialize.hpp>

namespace asge::game::components
{

/**
 * @brief Per-entity draw-order and screen-space controls for RenderSystem.
 *
 * Optional -- an entity with no RenderInfo of its own resolves as
 * RenderInfo{} (layer 0, no y-sort, world space). m_InheritSortFromParent
 * lets a Hierarchy subtree share its nearest ancestor's m_Layer, m_YSort
 * and sort owner instead of sorting independently; m_ScreenSpace always
 * propagates to descendants regardless. See RenderSystem.cpp's
 * ResolveRenderInfo for exactly how ancestors are walked.
 */
struct RenderInfo
{
    int  m_Layer                {0};     // Draw-order bucket; higher layers draw on top
    bool m_YSort                {false}; // Opt into sorting by bottom-edge Y within the layer
    bool m_ScreenSpace          {false}; // Draws in screen space, on top of every world-space entity; propagates to children regardless of m_InheritSortFromParent
    bool m_InheritSortFromParent{false}; // Adopt the nearest ancestor's resolved m_Layer/m_YSort/sort owner instead of this entity's own
    int  m_LocalOrder           {0};     // Tie-break among entities sharing the same sort owner; -1 means show behind the parent
};

}

namespace asge::game::scene
{

/** @brief Round-trips every RenderInfo field verbatim -- no entity references or runtime-only state to reconcile through a SaveContext/LoadContext. */
template<>
struct Serializer<components::RenderInfo>
{
    static constexpr str::StringView kTableName = "RenderInfo";

    using T = components::RenderInfo;

    static void ToToml(
                            T inValue,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    LoadContext const& inCtx ) noexcept;
};

}