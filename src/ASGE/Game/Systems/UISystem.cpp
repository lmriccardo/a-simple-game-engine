#include "UISystem.hpp"

#include <algorithm>
#include <tuple>

#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Game/Components/UI/UISlider.hpp>
#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Game/Resources/HitEntry.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>

namespace
{

using namespace asge::game;
using namespace asge::ecs;
using InteractableComponents = std::tuple<
    asge::game::components::UIButton, asge::game::components::UICheckbox,
    asge::game::components::UISlider>;

template<typename T>
void ResolveInteractionsImpl( 
    [[maybe_unused]]    Registry& inReg, 
    [[maybe_unused]]    Entity inEntity, 
    [[maybe_unused]]    T& ) noexcept {}

void ResolveInteractionsImpl( 
    [[maybe_unused]] Registry& inReg, [[maybe_unused]] Entity inEntity, 
    [[maybe_unused]] asge::game::components::UIButton& inButton ) noexcept
{
    inButton.m_OnClick.Emit();
}

void ResolveInteractionsImpl( 
    [[maybe_unused]] Registry& inReg, [[maybe_unused]] Entity inEntity, 
    asge::game::components::UICheckbox& inCheckbox ) noexcept
{
    inCheckbox.m_Checked = !inCheckbox.m_Checked;
    inCheckbox.m_OnToggled.Emit(inCheckbox.m_Checked);
}

template<std::size_t... Is>
void ResolveInteractionsImpl( 
    Registry& inReg, Entity inEntity, std::index_sequence<Is...> ) noexcept
{
    (
        [&]()
        {
            using ComponentType = std::tuple_element_t<Is, InteractableComponents>;
            if ( auto c = inReg.GetComponent<ComponentType>(inEntity) )
            {
                ResolveInteractionsImpl( inReg, inEntity, c.Value().get() );
            }
        }(), ...
    );
}

void ResolveInteractions( Registry& inReg, Entity inEntity ) noexcept
{
    static constexpr std::size_t N = std::tuple_size_v<InteractableComponents>;
    ResolveInteractionsImpl( inReg, inEntity, std::make_index_sequence<N>{} );
};

}

void asge::game::systems::UIInteractionSystem(
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
    ecs::Entity held    = ecs::Entity::Null(); // at most one -- a press only lands on the frontmost hovered entity
    for ( auto [ entity, interactable ] : inReg.View<components::Interactable>() )
    {
        auto& i = interactable.get();
        i.m_Clicked = false; // edge-triggered -- true only the frame a click actually completes, below

        if ( !i.m_Enabled )
        {
            i.m_Hovered = false;
            i.m_Held    = false;
            continue;
        }

        i.m_Hovered = ( entity == hovered );

        if ( justPressed && i.m_Hovered ) i.m_Held = true;
        if ( justReleased )
        {
            if ( i.m_Held && i.m_Hovered )
            {
                i.m_Clicked = true;
                clicked = entity;
            }
            i.m_Held = false;
        }

        if ( i.m_Held ) held = entity;
    }

    // Sliders emit continuously while held (press + drag), not on click
    // completion: map the pointer's x across the hit rect onto [m_Min, m_Max].
    // Held doesn't require hovered, so a drag survives the cursor leaving the rect.
    if ( held != ecs::Entity::Null() )
    {
        auto slider = inReg.GetComponent<components::UISlider>( held );
        auto const entry = std::ranges::find( entries, held, &resources::HitEntry::m_Entity );
        if ( slider && entry != entries.end() )
        {
            auto& s = slider.Value().get();
            float const mouseX = entry->m_ScreenSpace ? screenMouse.x() : worldMouse.x();
            float const t = entry->m_Rect.m_Width > 0.0f
                ? std::clamp( ( mouseX - entry->m_Rect.m_X ) / entry->m_Rect.m_Width, 0.0f, 1.0f ) : 0.0f;
            float const value = s.m_Min + t * ( s.m_Max - s.m_Min );

            if ( value != s.m_Value )
            {
                s.m_Value = value;
                s.m_OnValueChanged.Emit( value );
            }
        }
    }

    // Resolved interactions for those entities that needs a complete click
    // to emit the actual signal back to the caller.
    if ( clicked != ecs::Entity::Null() )
    {
        ResolveInteractions( inReg, clicked );
    }
}