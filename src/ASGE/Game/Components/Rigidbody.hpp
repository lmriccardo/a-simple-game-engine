#pragma once

#include <ASGE/Core/Strings.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

/**
 * @brief Marks an entity as physics-driven: subject to gravity and
 * collision-resolution push-out, as opposed to entities that merely have
 * a Velocity (e.g. scripted movement) without being a physics body.
 */
struct Rigidbody
{
    float m_Mass{1.0f};
    bool  m_AffectedByGravity{true};
};

template<>
struct Serializer<Rigidbody>
{
    using T = Rigidbody;
    static constexpr str::StringView kTableName = "Rigidbody";

    static void ToToml( 
        Rigidbody inRigidbody, asge::config::TOMLTableView inTview 
    ) noexcept {
        inTview.Table(std::string(kTableName))
               .Set("m_Mass", inRigidbody.m_Mass)
               .Set("m_AffectedByGravity", inRigidbody.m_AffectedByGravity);
    }

    static T FromToml( asge::config::TOMLTableView inEnttView ) noexcept
    {
        auto table = inEnttView.Table(std::string(kTableName));
        Rigidbody result{};
        result.m_Mass = table.Get("m_Mass", result.m_Mass);
        result.m_AffectedByGravity = table.Get("m_AffectedByGravity", result.m_AffectedByGravity);
        return result;
    }
};

}