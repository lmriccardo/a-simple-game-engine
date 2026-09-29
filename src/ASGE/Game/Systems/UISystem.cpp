#include "UISystem.hpp"

#include <algorithm>
#include <optional>
#include <tuple>
#include <variant>

#include <ASGE/Core/ECS/Hierarchy.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Game/Components/UI/UISlider.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <ASGE/Game/Components/UI/UILayoutItem.hpp>
#include <ASGE/Game/Components/UI/UIPanel.hpp>
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

/** @brief Size of each of inCount equal slots across inExtent with inGap between neighbours, never negative. */
float SlotExtent( float inExtent, int inCount, float inGap ) noexcept
{
    return std::max( 0.0f, ( inExtent - inGap * static_cast<float>( inCount - 1 ) ) / static_cast<float>( inCount ) );
}

std::optional<asge::math::Rect> SlotRect(
    asge::game::components::LayoutGrid const& inGrid, std::size_t inIndex,
    asge::math::Float2 inSize, asge::math::Float2 inGap ) noexcept
{
    if ( inGrid.m_Rows <= 0 || inGrid.m_Cols <= 0 ) return std::nullopt;
    if ( inIndex >= static_cast<std::size_t>( inGrid.m_Rows ) * static_cast<std::size_t>( inGrid.m_Cols ) )
        return std::nullopt;

    float const w = SlotExtent( inSize.x(), inGrid.m_Cols, inGap.x() );
    float const h = SlotExtent( inSize.y(), inGrid.m_Rows, inGap.y() );
    auto const cols = static_cast<std::size_t>( inGrid.m_Cols );
    return asge::math::Rect{
        static_cast<float>( inIndex % cols ) * ( w + inGap.x() ),
        static_cast<float>( inIndex / cols ) * ( h + inGap.y() ), w, h };
}

std::optional<asge::math::Rect> SlotRect(
    asge::game::components::LayoutVStack const& inVstack, std::size_t inIndex,
    asge::math::Float2 inSize, asge::math::Float2 inGap ) noexcept
{
    if ( inVstack.m_Rows <= 0 || inIndex >= static_cast<std::size_t>( inVstack.m_Rows ) ) return std::nullopt;

    float const h = SlotExtent( inSize.y(), inVstack.m_Rows, inGap.y() );
    return asge::math::Rect{ 0.0f, static_cast<float>( inIndex ) * ( h + inGap.y() ), inSize.x(), h };
}

std::optional<asge::math::Rect> SlotRect(
    asge::game::components::LayoutHStack const& inHstack, std::size_t inIndex,
    asge::math::Float2 inSize, asge::math::Float2 inGap ) noexcept
{
    if ( inHstack.m_Cols <= 0 || inIndex >= static_cast<std::size_t>( inHstack.m_Cols ) ) return std::nullopt;

    float const w = SlotExtent( inSize.x(), inHstack.m_Cols, inGap.x() );
    return asge::math::Rect{ static_cast<float>( inIndex ) * ( w + inGap.x() ), 0.0f, w, inSize.y() };
}

std::optional<asge::math::Rect> SlotRect(
    [[maybe_unused]] asge::game::components::LayoutAbsolute const& inAbsolute, [[maybe_unused]] std::size_t inIndex,
    [[maybe_unused]] asge::math::Float2 inSize, [[maybe_unused]] asge::math::Float2 inGap ) noexcept
{
    return std::nullopt;
}

/**
 * @brief The inIndex-th slot of inLayout (position and size, relative to the
 *        content area's top-left) inside an inSize area with inGap between
 *        slots, or nullopt if it has none (Absolute, a non-positive
 *        row/column count, or more children than slots).
 */
std::optional<asge::math::Rect> SlotRect(
    asge::game::components::LayoutSpec const& inLayout, std::size_t inIndex,
    asge::math::Float2 inSize, asge::math::Float2 inGap ) noexcept
{
    return std::visit(
        [&]( auto const& spec ){ return SlotRect( spec, inIndex, inSize, inGap ); },
        inLayout );
}

/** @brief Offset of a child inFree units smaller than its slot along one axis, never negative. */
float AlignOffset( asge::game::components::SlotAlign inAlign, float inFree ) noexcept
{
    using asge::game::components::SlotAlign;
    if ( inFree <= 0.0f ) return 0.0f;
    switch ( inAlign )
    {
    case SlotAlign::Start:  return 0.0f;
    case SlotAlign::Center: return inFree / 2.0f;
    case SlotAlign::End:    return inFree;
    }

    return 0.0f;
}

/**
 * @brief Moves inChild into inSlot (offset by inOrigin, in its parent's local
 *        space), stretching its UIRect on each axis its UILayoutItem fills.
 *
 * Without a UILayoutItem the child fills both axes and sits at the slot's
 * start. A Sprite is drawn at its texture's size and an auto-sized UILabel fits
 * its text, so neither is ever stretched. An axis that isn't stretched keeps
 * the child's size and is placed by the item's alignment. The rect is divided
 * by the child's local scale because UIRect::m_Size is pre-scale. A child with
 * no UIRect has no known size, so it is simply placed at the slot's start.
 */
void ApplySlot(
    asge::ecs::Registry& inReg, asge::ecs::Entity inChild, asge::game::components::Transform& inTransform,
    asge::math::Float2 inOrigin, asge::math::Rect const& inSlot ) noexcept
{
    namespace gc = asge::game::components;

    auto const itemR = inReg.GetComponent<gc::UILayoutItem>( inChild );
    gc::UILayoutItem const item = itemR.IsOk() ? itemR.Value().get() : gc::UILayoutItem{};

    auto rectR = inReg.GetComponent<gc::UIRect>( inChild );
    auto label = inReg.GetComponent<gc::UILabel>( inChild );
    bool const selfSized = inReg.GetComponent<gc::Sprite>( inChild ).IsOk()
                        || ( label.IsOk() && label.Value().get().m_AutoSize );

    float const sx = inTransform.m_LocalScale.x(), sy = inTransform.m_LocalScale.y();
    float drawnW = 0.0f, drawnH = 0.0f; // on-screen extent within the slot's space; 0 if it has no UIRect

    if ( rectR )
    {
        auto& size = rectR.Value().get().m_Size;
        if ( item.m_FillX && !selfSized && sx != 0.0f ) size = asge::math::Float2{ inSlot.m_Width / sx, size.y() };
        if ( item.m_FillY && !selfSized && sy != 0.0f ) size = asge::math::Float2{ size.x(), inSlot.m_Height / sy };
        drawnW = size.x() * sx;
        drawnH = size.y() * sy;
    }

    asge::math::Float2 const pos{
        inOrigin.x() + inSlot.m_X + ( rectR ? AlignOffset( item.m_AlignX, inSlot.m_Width - drawnW ) : 0.0f ),
        inOrigin.y() + inSlot.m_Y + ( rectR ? AlignOffset( item.m_AlignY, inSlot.m_Height - drawnH ) : 0.0f ) };

    // Only re-dirty on an actual move, so a settled layout stays free
    if ( inTransform.m_LocalCoordinates.x() != pos.x() || inTransform.m_LocalCoordinates.y() != pos.y() )
    {
        inTransform.m_LocalCoordinates = pos;
        inTransform.m_Dirty = true;
    }
}

/**
 * @brief Lays out inPanel's children into its slots, then recurses so a
 *        nested panel is laid out after the rect its parent just gave it.
 */
void LayoutChildren( asge::ecs::Registry& inReg, asge::ecs::Entity inPanel ) noexcept
{
    namespace gc = asge::game::components;
    auto panelR = inReg.GetComponent<gc::UIPanel>( inPanel );
    auto rectR  = inReg.GetComponent<gc::UIRect>( inPanel );
    auto hierR  = inReg.GetComponent<asge::ecs::components::Hierarchy>( inPanel );
    if ( !panelR || !rectR || !hierR ) return;

    auto const& panel = panelR.Value().get();
    auto const size = rectR.Value().get().m_Size;

    // Slots live in the panel's local space: inset by margin + padding on every side
    asge::math::Float2 const inset{
        panel.m_Margin.x() + panel.m_Padding.x(), panel.m_Margin.y() + panel.m_Padding.y() };
    asge::math::Float2 const content{
        std::max( 0.0f, size.x() - 2.0f * inset.x() ), std::max( 0.0f, size.y() - 2.0f * inset.y() ) };

    // Only children carrying a Transform take up a slot, in sibling order
    std::size_t index = 0;
    for ( auto child = hierR.Value().get().m_FirstChild; child != asge::ecs::Entity::Null(); )
    {
        auto childH = inReg.GetComponent<asge::ecs::components::Hierarchy>( child );
        if ( !childH ) break;
        auto const next = childH.Value().get().m_NextSibling;

        if ( auto t = inReg.GetComponent<gc::Transform>( child ) )
        {
            if ( auto slot = SlotRect( panel.m_Layout, index, content, panel.m_Spacing ) )
                ApplySlot( inReg, child, t.Value().get(), inset, *slot );
            ++index;
        }

        LayoutChildren( inReg, child ); // a no-op unless child is itself a panel
        child = next;
    }
}

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

void asge::game::systems::UILayoutSystem(ecs::Registry &inReg) noexcept
{
    // Start from panels that aren't inside another panel; nested ones are
    // reached (and sized) through their parent so they never lag a frame behind it.
    for ( [[maybe_unused]] auto [ e, p ] : inReg.View<components::UIPanel>() )
    {
        if ( auto h = inReg.GetComponent<ecs::components::Hierarchy>( e ) )
        {
            auto const parent = h.Value().get().m_Parent;
            if ( parent != ecs::Entity::Null() && inReg.GetComponent<components::UIPanel>( parent ).IsOk() )
                continue;
        }

        LayoutChildren( inReg, e );
    }
}
