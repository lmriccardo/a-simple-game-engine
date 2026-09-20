#pragma once

#include <ASGE/Core/Strings.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief 2D spatial transform — position, rotation, and scale.
 */
struct Transform
{
    float m_X{0.0f};
    float m_Y{0.0f};
    float m_Rotation{0.0f}; // radians
    float m_ScaleX{1.0f};
    float m_ScaleY{1.0f};
};

template<>
struct Serializer<Transform>
{
    using T = Transform;

    // The subtable name ToToml/FromToml agree on — also what a generic
    // per-entity walker checks (TOMLTableView::HasTable) to tell whether a
    // saved entity has this component, without hardcoding the name again.
    static constexpr str::StringView kTableName = "Transform";

    static void ToToml(
                            Transform inTransform,
                            asge::config::toml::TOMLTableView inTview,
        [[maybe_unused]]    scene::SaveContext const& inCtx ) noexcept;

    static T FromToml(
                            asge::config::toml::TOMLTableView inEnttView,
        [[maybe_unused]]    scene::LoadContext const& inCtx ) noexcept;
};

}