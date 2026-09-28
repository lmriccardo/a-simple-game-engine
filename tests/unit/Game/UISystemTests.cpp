#include <ASGE/Game/Systems/UISystem.hpp>
#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Game/Resources/HitEntry.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Events/Events.hpp>

#include <gtest/gtest.h>

#include <optional>

namespace
{

using asge::ecs::Entity;
using asge::ecs::Registry;
using asge::event::MouseButtonEvent;
using asge::event::MouseMotionEvent;
using asge::event::SystemEvent;
using asge::game::components::Interactable;
using asge::game::components::UIButton;
using asge::game::components::UICheckbox;
using asge::game::resources::HitEntry;
using asge::game::resources::UIHitList;
using asge::input::InputState;
using asge::input::MouseButton;
using asge::math::Rect;
using asge::video::Camera;

SystemEvent MotionEvent(float inX, float inY)
{
    MouseMotionEvent e{};
    e.s_Position = { inX, inY };
    return SystemEvent{ std::move(e) };
}

SystemEvent MouseButtonEv(MouseButton inButton, bool inDown)
{
    MouseButtonEvent e{};
    e.s_Button = inButton;
    e.s_Down = inDown;
    return SystemEvent{ std::move(e) };
}

/** @brief Moves the mouse to (inX, inY), as a settled position from a prior frame (not a fresh motion event this frame). */
InputState InputAt(float inX, float inY)
{
    InputState state;
    state.Consume( MotionEvent(inX, inY) );
    state.NewFrame();
    return state;
}

/** @brief Creates a full "Button" (UIButton + Interactable) -- UIRect is unnecessary here since these tests inject UIHitList entries directly rather than going through CollectHitList. */
Entity AddButton( Registry& inRegistry )
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), UIButton{}).IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), Interactable{}).IsOk());
    return entity.Value();
}

UIButton& Button( Registry& inRegistry, Entity inEntity )
{
    return inRegistry.GetComponent<UIButton>( inEntity ).Value().get();
}

/** @brief Creates a full "Checkbox" (UICheckbox + Interactable) -- see AddButton's own doc comment for why no UIRect. */
Entity AddCheckbox( Registry& inRegistry )
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), UICheckbox{}).IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), Interactable{}).IsOk());
    return entity.Value();
}

UICheckbox& Checkbox( Registry& inRegistry, Entity inEntity )
{
    return inRegistry.GetComponent<UICheckbox>( inEntity ).Value().get();
}

Interactable& Interact( Registry& inRegistry, Entity inEntity )
{
    return inRegistry.GetComponent<Interactable>( inEntity ).Value().get();
}

constexpr Camera kIdentityCamera{ .m_X = 0.0f, .m_Y = 0.0f, .m_Zoom = 1.0f };

// ─── No UIHitList resource ──────────────────────────────────────────────────

TEST(UIInteractionSystemTest, NoUIHitListResourceSet_LeavesEveryInteractableUntouched)
{
    Registry registry;
    auto const entity = AddButton( registry );
    Interact(registry, entity).m_Hovered = true; // pre-existing state the system must not touch

    InputState const input = InputAt( 0.0f, 0.0f );
    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_TRUE( Interact(registry, entity).m_Hovered ); // unchanged -- the system never ran
}

// ─── Hover resolution ───────────────────────────────────────────────────────

TEST(UIInteractionSystemTest, PointerOverScreenSpaceButton_SetsHovered)
{
    Registry registry;
    auto const entity = AddButton( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    InputState const input = InputAt( 50.0f, 25.0f ); // inside the rect
    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_TRUE( Interact(registry, entity).m_Hovered );
}

TEST(UIInteractionSystemTest, PointerOutsideEveryRect_HoveredIsFalse)
{
    Registry registry;
    auto const entity = AddButton( registry );
    Interact(registry, entity).m_Hovered = true; // must be cleared, not just left alone
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    InputState const input = InputAt( 500.0f, 500.0f );
    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FALSE( Interact(registry, entity).m_Hovered );
}

TEST(UIInteractionSystemTest, InteractableWithNoMatchingHitEntry_NeverHovered)
{
    // An Interactable the hit list simply doesn't mention (e.g. RenderSystem
    // culled it out this frame) must not stay stuck hovered from before.
    Registry registry;
    auto const entity = AddButton( registry );
    Interact(registry, entity).m_Hovered = true;
    registry.SetResource( UIHitList{} ); // empty -- resource present, no entries

    InputState const input = InputAt( 0.0f, 0.0f );
    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FALSE( Interact(registry, entity).m_Hovered );
}

TEST(UIInteractionSystemTest, OverlappingEntries_LastInTheListWinsAsTheTopmost)
{
    // UIHitList is documented back-to-front, so the last entry is whatever
    // drew on top -- ties over the same pointer position must resolve to it.
    Registry registry;
    auto const back = AddButton( registry );
    auto const front = AddButton( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ back,  Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, true },
        HitEntry{ front, Rect{ 0.0f, 0.0f, 100.0f, 100.0f }, true }, // same rect, drawn later
    } } );

    InputState const input = InputAt( 50.0f, 50.0f );
    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_TRUE( Interact(registry, front).m_Hovered );
    EXPECT_FALSE( Interact(registry, back).m_Hovered );
}

// ─── Disabled Interactable ───────────────────────────────────────────────────

TEST(UIInteractionSystemTest, DisabledInteractable_ForcedHoveredAndHeldFalseRegardlessOfHitList)
{
    Registry registry;
    auto const entity = AddButton( registry );
    Interact(registry, entity).m_Enabled = false;
    Interact(registry, entity).m_Held = true; // as if disabled mid-press
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    InputState const input = InputAt( 50.0f, 25.0f ); // inside the rect
    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FALSE( Interact(registry, entity).m_Hovered );
    EXPECT_FALSE( Interact(registry, entity).m_Held );
}

// ─── Screen space vs. world space ───────────────────────────────────────────

TEST(UIInteractionSystemTest, ScreenSpaceEntry_TestedAgainstRawMousePosition)
{
    Registry registry;
    auto const entity = AddButton( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 30.0f, 10.0f, 20.0f, 20.0f }, true } // covers raw (40,20)
    } } );

    // A non-trivial camera must not affect a screen-space entry at all.
    Camera const camera{ .m_X = 1000.0f, .m_Y = 1000.0f, .m_Zoom = 4.0f };
    InputState const input = InputAt( 40.0f, 20.0f );
    asge::game::systems::UIInteractionSystem( registry, input, camera );

    EXPECT_TRUE( Interact(registry, entity).m_Hovered );
}

TEST(UIInteractionSystemTest, WorldSpaceEntry_TestedAgainstCameraUnprojectedPosition)
{
    Registry registry;
    auto const entity = AddButton( registry );
    // camera.m_X + screenMouse/zoom = 100 + 40/2 = 120, 50 + 20/2 = 60.
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 110.0f, 50.0f, 20.0f, 20.0f }, false }
    } } );

    Camera const camera{ .m_X = 100.0f, .m_Y = 50.0f, .m_Zoom = 2.0f };
    InputState const input = InputAt( 40.0f, 20.0f ); // nowhere near the rect in raw screen space
    asge::game::systems::UIInteractionSystem( registry, input, camera );

    EXPECT_TRUE( Interact(registry, entity).m_Hovered );
}

// ─── Press / release / click ────────────────────────────────────────────────

TEST(UIInteractionSystemTest, PressWhileHovered_SetsHeld)
{
    Registry registry;
    auto const entity = AddButton( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    InputState input;
    input.Consume( MotionEvent(50.0f, 25.0f) );
    input.NewFrame();
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) ); // fresh press this frame

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_TRUE( Interact(registry, entity).m_Held );
}

TEST(UIInteractionSystemTest, PressWhileNotHovered_LeavesHeldFalse)
{
    Registry registry;
    auto const entity = AddButton( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    InputState input;
    input.Consume( MotionEvent(500.0f, 500.0f) ); // outside the rect
    input.NewFrame();
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) );

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FALSE( Interact(registry, entity).m_Held );
}

TEST(UIInteractionSystemTest, ReleaseWhileHeldAndHovered_SetsClickedFiresOnClickAndClearsHeld)
{
    Registry registry;
    auto const entity = AddButton( registry );
    Interact(registry, entity).m_Held = true; // as if UIInteractionSystem set it on a prior frame's press
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    bool clicked = false;
    Button(registry, entity).m_OnClick.Connect( [&clicked]{ clicked = true; } );

    InputState input;
    input.Consume( MotionEvent(50.0f, 25.0f) ); // still over the button
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) ); // down last frame...
    input.NewFrame();
    input.Consume( MouseButtonEv(MouseButton::LEFT, false) ); // ...released this one

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_TRUE( clicked );
    EXPECT_TRUE( Interact(registry, entity).m_Clicked );
    EXPECT_FALSE( Interact(registry, entity).m_Held );
}

TEST(UIInteractionSystemTest, MClickedIsEdgeTriggered_FalseAgainOnTheFollowingFrame)
{
    Registry registry;
    auto const entity = AddButton( registry );
    Interact(registry, entity).m_Clicked = true; // as if set by a completed click last frame
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    InputState const input = InputAt( 500.0f, 500.0f ); // no press/release this frame at all
    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FALSE( Interact(registry, entity).m_Clicked );
}

TEST(UIInteractionSystemTest, ReleaseWhileHeldButNoLongerHovered_DoesNotFireOnClick)
{
    // Pressed down on the button, dragged off it, then released -- a
    // cancelled click, not a completed one.
    Registry registry;
    auto const entity = AddButton( registry );
    Interact(registry, entity).m_Held = true;
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    bool clicked = false;
    Button(registry, entity).m_OnClick.Connect( [&clicked]{ clicked = true; } );

    InputState input;
    input.Consume( MotionEvent(500.0f, 500.0f) ); // moved off the button first
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) ); // down last frame...
    input.NewFrame();
    input.Consume( MouseButtonEv(MouseButton::LEFT, false) ); // ...released this one

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FALSE( clicked );
    EXPECT_FALSE( Interact(registry, entity).m_Clicked );
    EXPECT_FALSE( Interact(registry, entity).m_Held ); // still cleared regardless
}

TEST(UIInteractionSystemTest, ReleaseWhileNeverHeld_DoesNotFireOnClick)
{
    // A release with no prior press on this button (e.g. the drag started
    // elsewhere) must not be mistaken for a click just because it's hovered.
    Registry registry;
    auto const entity = AddButton( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    bool clicked = false;
    Button(registry, entity).m_OnClick.Connect( [&clicked]{ clicked = true; } );

    // A genuine down-then-up transition (so IsMouseButtonReleased is true),
    // just with Interactable::m_Held never having been set for this entity --
    // e.g. the press that started the drag landed on empty space.
    InputState input;
    input.Consume( MotionEvent(50.0f, 25.0f) );
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) );
    input.NewFrame();
    input.Consume( MouseButtonEv(MouseButton::LEFT, false) );

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FALSE( clicked );
}

// ─── UICheckbox toggling ─────────────────────────────────────────────────────

TEST(UIInteractionSystemTest, ReleaseWhileHeldAndHovered_TogglesCheckedAndFiresOnToggled)
{
    Registry registry;
    auto const entity = AddCheckbox( registry );
    Interact(registry, entity).m_Held = true; // as if UIInteractionSystem set it on a prior frame's press
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    std::optional<bool> toggledTo;
    Checkbox(registry, entity).m_OnToggled.Connect( [&toggledTo]( bool inChecked ){ toggledTo = inChecked; } );

    InputState input;
    input.Consume( MotionEvent(50.0f, 25.0f) ); // still over the checkbox
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) ); // down last frame...
    input.NewFrame();
    input.Consume( MouseButtonEv(MouseButton::LEFT, false) ); // ...released this one

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    ASSERT_TRUE( toggledTo.has_value() );
    EXPECT_TRUE( *toggledTo );
    EXPECT_TRUE( Checkbox(registry, entity).m_Checked );
    EXPECT_TRUE( Interact(registry, entity).m_Clicked );
}

TEST(UIInteractionSystemTest, TwoCompletedClicksInARow_TogglesBackToUnchecked)
{
    Registry registry;
    auto const entity = AddCheckbox( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    // Same held-then-released-while-hovered shape as ReleaseWhileHeldAndHovered_
    // TogglesCheckedAndFiresOnToggled above, run twice.
    auto const releaseWhileHeldAndHovered = [&]
    {
        Interact(registry, entity).m_Held = true; // as if a prior frame's press set it
        InputState input;
        input.Consume( MotionEvent(50.0f, 25.0f) );
        input.Consume( MouseButtonEv(MouseButton::LEFT, true) );
        input.NewFrame();
        input.Consume( MouseButtonEv(MouseButton::LEFT, false) );
        asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );
    };

    releaseWhileHeldAndHovered();
    EXPECT_TRUE( Checkbox(registry, entity).m_Checked );

    releaseWhileHeldAndHovered();
    EXPECT_FALSE( Checkbox(registry, entity).m_Checked );
}

TEST(UIInteractionSystemTest, ReleaseWhileHeldButNoLongerHovered_DoesNotToggleChecked)
{
    // Pressed down on the checkbox, dragged off it, then released -- a
    // cancelled click, not a completed one (see the equivalent UIButton test).
    Registry registry;
    auto const entity = AddCheckbox( registry );
    Interact(registry, entity).m_Held = true;
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 50.0f }, true }
    } } );

    InputState input;
    input.Consume( MotionEvent(500.0f, 500.0f) ); // moved off the checkbox first
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) );
    input.NewFrame();
    input.Consume( MouseButtonEv(MouseButton::LEFT, false) );

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FALSE( Checkbox(registry, entity).m_Checked );
}

}
