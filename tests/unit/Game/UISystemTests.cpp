#include <ASGE/Game/Systems/UISystem.hpp>
#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Game/Components/UI/UISlider.hpp>
#include <ASGE/Game/Components/UI/UIPanel.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <ASGE/Game/Components/UI/UILayoutItem.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Core/ECS/Hierarchy.hpp>
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
using asge::game::components::UISlider;
using asge::game::components::UIPanel;
using asge::game::components::UILabel;
using asge::game::components::UILayoutItem;
using asge::game::components::SlotAlign;
using asge::game::components::Sprite;
using asge::game::components::UIRect;
using asge::game::components::Transform;
using asge::game::components::LayoutSpec;
using asge::game::components::LayoutAbsolute;
using asge::game::components::LayoutGrid;
using asge::game::components::LayoutVStack;
using asge::game::components::LayoutHStack;
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

/** @brief Creates a full "Slider" (UISlider + Interactable, default 0..1 range) -- see AddButton's own doc comment for why no UIRect. */
Entity AddSlider( Registry& inRegistry )
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), UISlider{}).IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), Interactable{}).IsOk());
    return entity.Value();
}

UISlider& Slider( Registry& inRegistry, Entity inEntity )
{
    return inRegistry.GetComponent<UISlider>( inEntity ).Value().get();
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

// ─── UILayoutSystem ──────────────────────────────────────────────────────────

/** @brief Creates a 200x100 panel at the origin with inLayout and inPadding. */
Entity AddPanel( Registry& inRegistry, LayoutSpec inLayout, asge::math::Float2 inPadding = {} )
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), UIRect{ .m_Size = { 200.0f, 100.0f } }).IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), UIPanel{ .m_Layout = inLayout, .m_Padding = inPadding }).IsOk());
    return entity.Value();
}

/** @brief Creates a Transform-carrying entity and attaches it as the next child of inParent. */
Entity AddChildTo( Registry& inRegistry, Entity inParent )
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), Transform{}).IsOk());
    asge::ecs::components::AttachChild( inRegistry, inParent, entity.Value() );
    return entity.Value();
}

Transform& TransformOf( Registry& inRegistry, Entity inEntity )
{
    return inRegistry.GetComponent<Transform>( inEntity ).Value().get();
}

TEST(UILayoutSystemTest, Grid_PlacesChildrenRowMajorInEqualCells)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutGrid{ .m_Rows = 2, .m_Cols = 2 } ); // 100x50 cells
    auto const a = AddChildTo( registry, panel );
    auto const b = AddChildTo( registry, panel );
    auto const c = AddChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, a).m_LocalCoordinates.x(), 0.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, a).m_LocalCoordinates.y(), 0.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, b).m_LocalCoordinates.x(), 100.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, b).m_LocalCoordinates.y(), 0.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, c).m_LocalCoordinates.x(), 0.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, c).m_LocalCoordinates.y(), 50.0f );
}

TEST(UILayoutSystemTest, LastChild_IsLaidOutToo)
{
    // Regression: the sibling walk once stopped short of the last child.
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } );
    AddChildTo( registry, panel );
    auto const last = AddChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, last).m_LocalCoordinates.x(), 100.0f );
}

TEST(UILayoutSystemTest, VStack_StacksChildrenIntoEqualRows)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutVStack{ .m_Rows = 4 } ); // 25px rows
    AddChildTo( registry, panel );
    auto const second = AddChildTo( registry, panel );
    auto const third = AddChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, second).m_LocalCoordinates.x(), 0.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, second).m_LocalCoordinates.y(), 25.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, third).m_LocalCoordinates.y(), 50.0f );
}

TEST(UILayoutSystemTest, HStack_LinesChildrenUpIntoEqualColumns)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 4 } ); // 50px columns
    AddChildTo( registry, panel );
    auto const second = AddChildTo( registry, panel );
    auto const third = AddChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, second).m_LocalCoordinates.x(), 50.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, second).m_LocalCoordinates.y(), 0.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, third).m_LocalCoordinates.x(), 100.0f );
}

TEST(UILayoutSystemTest, Padding_InsetsTheSlotsOnEverySide)
{
    Registry registry;
    // 200x100 minus 10px padding on each side: a 180x80 content area, 90px columns.
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 }, { 10.0f, 10.0f } );
    auto const first = AddChildTo( registry, panel );
    auto const second = AddChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, first).m_LocalCoordinates.x(), 10.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, first).m_LocalCoordinates.y(), 10.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, second).m_LocalCoordinates.x(), 100.0f );
}

TEST(UILayoutSystemTest, ChildrenPastTheLastSlot_KeepTheirPosition)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } );
    AddChildTo( registry, panel );
    auto const extra = AddChildTo( registry, panel );
    TransformOf(registry, extra).m_LocalCoordinates = { 7.0f, 9.0f };
    TransformOf(registry, extra).m_Dirty = false;

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, extra).m_LocalCoordinates.x(), 7.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, extra).m_LocalCoordinates.y(), 9.0f );
    EXPECT_FALSE( TransformOf(registry, extra).m_Dirty );
}

TEST(UILayoutSystemTest, AbsolutePanel_LeavesChildrenUntouched)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutAbsolute{} );
    auto const child = AddChildTo( registry, panel );
    TransformOf(registry, child).m_LocalCoordinates = { 7.0f, 9.0f };
    TransformOf(registry, child).m_Dirty = false;

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.x(), 7.0f );
    EXPECT_FALSE( TransformOf(registry, child).m_Dirty );
}

TEST(UILayoutSystemTest, ChildWithoutTransform_DoesNotTakeASlot)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } );

    auto bare = registry.CreateEntity();
    ASSERT_TRUE( bare.IsOk() );
    asge::ecs::components::AttachChild( registry, panel, bare.Value() ); // no Transform
    auto const child = AddChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.x(), 0.0f ); // first slot, not second
}

/** @brief Like AddChildTo, but the child also carries a 5x5 UIRect for the layout to resize. */
Entity AddSizedChildTo( Registry& inRegistry, Entity inParent )
{
    auto const child = AddChildTo( inRegistry, inParent );
    EXPECT_TRUE(inRegistry.AddComponent(child, UIRect{ .m_Size = { 5.0f, 5.0f } }).IsOk());
    return child;
}

UIRect& RectOf( Registry& inRegistry, Entity inEntity )
{
    return inRegistry.GetComponent<UIRect>( inEntity ).Value().get();
}

TEST(UILayoutSystemTest, ChildRect_IsResizedToFillItsSlot)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutGrid{ .m_Rows = 2, .m_Cols = 2 } ); // 100x50 cells
    auto const child = AddSizedChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, child).m_Size.x(), 100.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, child).m_Size.y(), 50.0f );
}

TEST(UILayoutSystemTest, VStackAndHStack_StretchChildrenAcrossTheOtherAxis)
{
    Registry registry;
    auto const vpanel = AddPanel( registry, LayoutVStack{ .m_Rows = 2 } );
    auto const hpanel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } );
    auto const inV = AddSizedChildTo( registry, vpanel );
    auto const inH = AddSizedChildTo( registry, hpanel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, inV).m_Size.x(), 200.0f ); // full width, half height
    EXPECT_FLOAT_EQ( RectOf(registry, inV).m_Size.y(), 50.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, inH).m_Size.x(), 100.0f ); // half width, full height
    EXPECT_FLOAT_EQ( RectOf(registry, inH).m_Size.y(), 100.0f );
}

TEST(UILayoutSystemTest, Spacing_LeavesAGapBetweenSlotsAndShrinksThem)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } );
    registry.GetComponent<UIPanel>( panel ).Value().get().m_Spacing = { 20.0f, 0.0f }; // 200 - 20 = 90px slots
    auto const first = AddSizedChildTo( registry, panel );
    auto const second = AddSizedChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, first).m_Size.x(), 90.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, second).m_Size.x(), 90.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, second).m_LocalCoordinates.x(), 110.0f );
}

TEST(UILayoutSystemTest, GridSpacing_AppliesOnBothAxes)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutGrid{ .m_Rows = 2, .m_Cols = 2 } );
    registry.GetComponent<UIPanel>( panel ).Value().get().m_Spacing = { 20.0f, 10.0f }; // 90x45 cells
    AddSizedChildTo( registry, panel );
    AddSizedChildTo( registry, panel );
    AddSizedChildTo( registry, panel );
    auto const fourth = AddSizedChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, fourth).m_LocalCoordinates.x(), 110.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, fourth).m_LocalCoordinates.y(), 55.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, fourth).m_Size.x(), 90.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, fourth).m_Size.y(), 45.0f );
}

TEST(UILayoutSystemTest, ChildWithLocalScale_GetsPreScaleRectSoItStillFillsTheSlot)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } ); // 200x100 slot
    auto const child = AddSizedChildTo( registry, panel );
    TransformOf(registry, child).m_LocalScale = { 2.0f, 4.0f };

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, child).m_Size.x(), 100.0f ); // 100 * 2 = 200
    EXPECT_FLOAT_EQ( RectOf(registry, child).m_Size.y(), 25.0f );  // 25 * 4 = 100
}

TEST(UILayoutSystemTest, SpriteChild_IsMovedButKeepsItsOwnRectSize)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } );
    AddSizedChildTo( registry, panel );
    auto const sprite = AddSizedChildTo( registry, panel );
    ASSERT_TRUE( registry.AddComponent( sprite, Sprite{} ).IsOk() );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, sprite).m_LocalCoordinates.x(), 100.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, sprite).m_Size.x(), 5.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, sprite).m_Size.y(), 5.0f );
}

TEST(UILayoutSystemTest, AutoSizedLabel_KeepsItsSizeButAFixedSizeLabelIsResized)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } );
    auto const autoSized = AddSizedChildTo( registry, panel );
    auto const fixedSize = AddSizedChildTo( registry, panel );
    ASSERT_TRUE( registry.AddComponent( autoSized, UILabel{ .m_AutoSize = true } ).IsOk() );
    ASSERT_TRUE( registry.AddComponent( fixedSize, UILabel{ .m_AutoSize = false } ).IsOk() );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, autoSized).m_Size.x(), 5.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, fixedSize).m_Size.x(), 100.0f );
}

TEST(UILayoutSystemTest, NestedPanel_IsSizedByItsParentThenLaysOutItsOwnChildren)
{
    Registry registry;
    auto const outer = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } ); // 100x100 columns
    auto const inner = AddSizedChildTo( registry, outer );
    ASSERT_TRUE( registry.AddComponent( inner, UIPanel{ .m_Layout = LayoutVStack{ .m_Rows = 2 } } ).IsOk() );
    auto const grandchild = AddSizedChildTo( registry, inner );
    auto const second = AddSizedChildTo( registry, inner );

    asge::game::systems::UILayoutSystem( registry ); // a single call must settle both levels

    EXPECT_FLOAT_EQ( RectOf(registry, inner).m_Size.x(), 100.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, grandchild).m_Size.x(), 100.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, grandchild).m_Size.y(), 50.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, second).m_LocalCoordinates.y(), 50.0f );
}

TEST(UILayoutSystemTest, AbsoluteParent_StillLaysOutANestedPanel)
{
    Registry registry;
    auto const outer = AddPanel( registry, LayoutAbsolute{} );
    auto const inner = AddSizedChildTo( registry, outer );
    ASSERT_TRUE( registry.AddComponent( inner, UIPanel{ .m_Layout = LayoutHStack{ .m_Cols = 2 } } ).IsOk() );
    auto const grandchild = AddSizedChildTo( registry, inner );
    RectOf(registry, inner).m_Size = { 80.0f, 40.0f }; // the Absolute parent leaves this alone

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, inner).m_Size.x(), 80.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, grandchild).m_Size.x(), 40.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, grandchild).m_Size.y(), 40.0f );
}

/** @brief Adds a UILayoutItem to inChild. */
void SetItem( Registry& inRegistry, Entity inChild, UILayoutItem inItem )
{
    ASSERT_TRUE( inRegistry.AddComponent( inChild, inItem ).IsOk() );
}

TEST(UILayoutItemLayoutTest, NoFill_KeepsTheChildsOwnSize)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } );
    auto const child = AddSizedChildTo( registry, panel );
    SetItem( registry, child, UILayoutItem{ .m_FillX = false, .m_FillY = false } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, child).m_Size.x(), 5.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, child).m_Size.y(), 5.0f );
}

TEST(UILayoutItemLayoutTest, FillIsPerAxis)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } ); // 200x100 slot
    auto const child = AddSizedChildTo( registry, panel );
    SetItem( registry, child, UILayoutItem{ .m_FillX = true, .m_FillY = false } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, child).m_Size.x(), 200.0f );
    EXPECT_FLOAT_EQ( RectOf(registry, child).m_Size.y(), 5.0f );
}

TEST(UILayoutItemLayoutTest, Center_PlacesAnUnfilledChildInTheMiddleOfItsSlot)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } ); // 200x100 slot, 5x5 child
    auto const child = AddSizedChildTo( registry, panel );
    SetItem( registry, child, UILayoutItem{
        .m_FillX = false, .m_FillY = false, .m_AlignX = SlotAlign::Center, .m_AlignY = SlotAlign::Center } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.x(), 97.5f );
    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.y(), 47.5f );
}

TEST(UILayoutItemLayoutTest, End_PlacesAnUnfilledChildAgainstTheFarEdges)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } );
    auto const child = AddSizedChildTo( registry, panel );
    SetItem( registry, child, UILayoutItem{
        .m_FillX = false, .m_FillY = false, .m_AlignX = SlotAlign::End, .m_AlignY = SlotAlign::End } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.x(), 195.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.y(), 95.0f );
}

TEST(UILayoutItemLayoutTest, AlignmentAccountsForLocalScale)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } );
    auto const child = AddSizedChildTo( registry, panel ); // 5x5 pre-scale, drawn 20x20 at scale 4
    TransformOf(registry, child).m_LocalScale = { 4.0f, 4.0f };
    SetItem( registry, child, UILayoutItem{
        .m_FillX = false, .m_FillY = false, .m_AlignX = SlotAlign::Center, .m_AlignY = SlotAlign::Center } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.x(), 90.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.y(), 40.0f );
}

TEST(UILayoutItemLayoutTest, ChildLargerThanItsSlot_StartsAtTheSlotEdgeInsteadOfOverflowingBackwards)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } ); // 100x100 slots
    AddSizedChildTo( registry, panel );
    auto const wide = AddSizedChildTo( registry, panel );
    RectOf(registry, wide).m_Size = { 300.0f, 5.0f };
    SetItem( registry, wide, UILayoutItem{
        .m_FillX = false, .m_FillY = false, .m_AlignX = SlotAlign::Center, .m_AlignY = SlotAlign::Start } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, wide).m_LocalCoordinates.x(), 100.0f );
}

TEST(UILayoutItemLayoutTest, FillingAxisIgnoresItsAlignment)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } );
    auto const child = AddSizedChildTo( registry, panel );
    SetItem( registry, child, UILayoutItem{ .m_AlignX = SlotAlign::End, .m_AlignY = SlotAlign::End } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.x(), 0.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, child).m_LocalCoordinates.y(), 0.0f );
}

TEST(UILayoutItemLayoutTest, SpriteWithFillRequested_IsStillNeverStretchedButIsAligned)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 1 } );
    auto const sprite = AddSizedChildTo( registry, panel );
    ASSERT_TRUE( registry.AddComponent( sprite, Sprite{} ).IsOk() );
    SetItem( registry, sprite, UILayoutItem{ .m_AlignX = SlotAlign::Center, .m_AlignY = SlotAlign::Center } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( RectOf(registry, sprite).m_Size.x(), 5.0f );
    EXPECT_FLOAT_EQ( TransformOf(registry, sprite).m_LocalCoordinates.x(), 97.5f );
}

TEST(UILayoutItemLayoutTest, ChildWithoutARect_IsPlacedAtTheSlotStartRegardlessOfAlignment)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } );
    AddChildTo( registry, panel );
    auto const bare = AddChildTo( registry, panel ); // Transform only: no size to align by
    SetItem( registry, bare, UILayoutItem{ .m_AlignX = SlotAlign::End } );

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FLOAT_EQ( TransformOf(registry, bare).m_LocalCoordinates.x(), 100.0f ); // slot start, not 200
}

TEST(UILayoutSystemTest, SettledLayout_DoesNotMarkChildrenDirtyAgain)
{
    Registry registry;
    auto const panel = AddPanel( registry, LayoutHStack{ .m_Cols = 2 } );
    AddChildTo( registry, panel );
    auto const second = AddChildTo( registry, panel );

    asge::game::systems::UILayoutSystem( registry );
    ASSERT_TRUE( TransformOf(registry, second).m_Dirty );
    TransformOf(registry, second).m_Dirty = false; // as TransformPropagationSystem would

    asge::game::systems::UILayoutSystem( registry );

    EXPECT_FALSE( TransformOf(registry, second).m_Dirty );
}

// ─── UISlider dragging ───────────────────────────────────────────────────────

TEST(UIInteractionSystemTest, PressOnSlider_JumpsValueToPointerAndFiresOnValueChanged)
{
    Registry registry;
    auto const entity = AddSlider( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 100.0f, 0.0f, 200.0f, 20.0f }, true }
    } } );

    std::optional<float> changedTo;
    Slider(registry, entity).m_OnValueChanged.Connect( [&changedTo]( float inValue ){ changedTo = inValue; } );

    InputState input;
    input.Consume( MotionEvent(150.0f, 10.0f) ); // a quarter along the 200px track
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) );

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    ASSERT_TRUE( changedTo.has_value() );
    EXPECT_FLOAT_EQ( *changedTo, 0.25f );
    EXPECT_FLOAT_EQ( Slider(registry, entity).m_Value, 0.25f );
}

TEST(UIInteractionSystemTest, HeldSliderDraggedOffItsRect_ClampsToTheEndOfTheRange)
{
    Registry registry;
    auto const entity = AddSlider( registry );
    Interact(registry, entity).m_Held = true;
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 100.0f, 0.0f, 200.0f, 20.0f }, true }
    } } );

    InputState input = InputAt( 900.0f, 500.0f ); // far right and below, still held

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FLOAT_EQ( Slider(registry, entity).m_Value, 1.0f );
}

TEST(UIInteractionSystemTest, HeldSliderDraggedLeftOfItsRect_ClampsToMin)
{
    Registry registry;
    auto const entity = AddSlider( registry );
    Slider(registry, entity).m_Value = 0.7f;
    Interact(registry, entity).m_Held = true;
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 100.0f, 0.0f, 200.0f, 20.0f }, true }
    } } );

    InputState input = InputAt( 0.0f, 10.0f );

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FLOAT_EQ( Slider(registry, entity).m_Value, 0.0f );
}

TEST(UIInteractionSystemTest, HeldSliderPointerUnmoved_DoesNotFireOnValueChangedAgain)
{
    Registry registry;
    auto const entity = AddSlider( registry );
    Slider(registry, entity).m_Value = 0.5f;
    Interact(registry, entity).m_Held = true;
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 100.0f, 0.0f, 200.0f, 20.0f }, true }
    } } );

    int fired = 0;
    Slider(registry, entity).m_OnValueChanged.Connect( [&fired]( float ){ ++fired; } );

    InputState input = InputAt( 200.0f, 10.0f ); // exactly the current value's x

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_EQ( fired, 0 );
}

TEST(UIInteractionSystemTest, SliderNotHeld_IgnoresPointerMovement)
{
    Registry registry;
    auto const entity = AddSlider( registry );
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 100.0f, 0.0f, 200.0f, 20.0f }, true }
    } } );

    InputState input = InputAt( 250.0f, 10.0f ); // hovering only, no press

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FLOAT_EQ( Slider(registry, entity).m_Value, 0.0f );
}

TEST(UIInteractionSystemTest, SliderWithCustomRange_MapsPointerOntoMinToMax)
{
    Registry registry;
    auto const entity = AddSlider( registry );
    Slider(registry, entity).m_Min = 10.0f;
    Slider(registry, entity).m_Max = 20.0f;
    Interact(registry, entity).m_Held = true;
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 0.0f, 0.0f, 100.0f, 20.0f }, true }
    } } );

    InputState input = InputAt( 50.0f, 10.0f );

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_FLOAT_EQ( Slider(registry, entity).m_Value, 15.0f );
}

TEST(UIInteractionSystemTest, CompletedClickOnSlider_DoesNotFireOnValueChangedOnRelease)
{
    // Sliders emit while held, not on click completion -- the release frame
    // with an unmoved pointer must not fire a second, stale emit.
    Registry registry;
    auto const entity = AddSlider( registry );
    Slider(registry, entity).m_Value = 0.5f;
    Interact(registry, entity).m_Held = true;
    registry.SetResource( UIHitList{ .m_Entries = {
        HitEntry{ entity, Rect{ 100.0f, 0.0f, 200.0f, 20.0f }, true }
    } } );

    int fired = 0;
    Slider(registry, entity).m_OnValueChanged.Connect( [&fired]( float ){ ++fired; } );

    InputState input;
    input.Consume( MotionEvent(200.0f, 10.0f) );
    input.Consume( MouseButtonEv(MouseButton::LEFT, true) );
    input.NewFrame();
    input.Consume( MouseButtonEv(MouseButton::LEFT, false) );

    asge::game::systems::UIInteractionSystem( registry, input, kIdentityCamera );

    EXPECT_EQ( fired, 0 );
}

}
