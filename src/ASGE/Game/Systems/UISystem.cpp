#include "UISystem.hpp"

#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Resources/HitEntry.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

void asge::game::systems::UIButtonSystem(
    ecs::Registry &inReg, input::InputState const &inInput, video::Camera const &inCamera)
{
    auto hitList = inReg.GetResource<resources::UIHitList>();
    if ( !hitList ) return;

    math::Float2 const screenMouse = inInput.GetMousePosition();
    math::Float2 const worldMouse {
        inCamera.m_X + screenMouse.x() / inCamera.m_Zoom,
        inCamera.m_Y + screenMouse.y() / inCamera.m_Zoom
    };

    ecs::Entity hovered = ecs::Entity::Null();
    auto const& entries = hitList.Value().get().m_Entries;
    for ( auto it = entries.rbegin(); it != entries.rend(); ++it )
    {
        math::Float2 const& mouse = it->m_ScreenSpace ? screenMouse : worldMouse;
        if ( math::Contains( it->m_Rect, mouse ) )
        {
            hovered = it->m_Entity;
            break;
        }
    }

    bool const justPressed  = inInput.IsMouseButtonPressed( input::MouseButton::LEFT );
    bool const justReleased = inInput.IsMouseButtonReleased( input::MouseButton::LEFT );

    ecs::Entity clicked = ecs::Entity::Null();
    for ( auto [ entity, button ] : inReg.View<components::UIButton>() )
    {
        auto& b = button.get();
        b.m_Hovered = ( entity == hovered );

        if ( justPressed && b.m_Hovered ) b.m_Held = true;
        if ( justReleased )
        {
            if ( b.m_Held && b.m_Hovered ) clicked = entity;
            b.m_Held = false;
        }
    }

    if ( clicked != ecs::Entity::Null() )
    {
        if ( auto b = inReg.GetComponent<components::UIButton>( clicked ) )
        {
            b.Value().get().m_OnClick.Emit();
        }
    }
}