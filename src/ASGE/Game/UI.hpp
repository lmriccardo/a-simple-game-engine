#pragma once

#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <functional>

namespace asge::game::ui
{

namespace consts
{

static constexpr math::Float2 k_ButtonSize = math::Float2{80.0f, 24.0f};
static constexpr components::details::StateColors k_StateColor = 
    components::details::StateColors{};

}

// ecs::Entity CreateButton(
//     ecs::Registry& inReg, str::StringCRef Name = {}, str::StringCRef inText = {},

// );

// ecs::Entity CreateLabel(
//     ecs::Registry& inReg,
// );

}