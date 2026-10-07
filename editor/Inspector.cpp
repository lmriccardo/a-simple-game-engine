#include "Inspector.hpp"

#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Velocity.hpp>
#include <ASGE/Game/Components/Rigidbody.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Collider.hpp>
#include <ASGE/Game/Components/Camera.hpp>
#include <ASGE/Game/Components/AudioSource.hpp>
#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/PathFollow.hpp>
#include <ASGE/Game/Components/Name.hpp>
#include <ASGE/Game/Components/Hierarchy.hpp>
#include <ASGE/Game/Components/RenderInfo.hpp>
#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Game/Components/UI/UISlider.hpp>
#include <ASGE/Game/Components/UI/UIPanel.hpp>
#include <ASGE/Game/Components/UI/UILayoutItem.hpp>
#include <ASGE/Core/ECS/Markers.hpp>

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

using namespace asge::game::components;

namespace
{

// inEntity.m_Index is a recyclable ECS storage slot (FreeList reuses freed
// indices LIFO -- see its own doc comment), not a creation-order counter --
// using it directly (for the "Entity #N" fallback label, or for list
// ordering via Registry::AllEntities(), itself just a raw slot scan) made
// both jump around non-monotonically after any delete. This assigns every
// entity (named or not, index+generation so a recycled slot's new
// generation is correctly a fresh key) a stable, ever-increasing id the
// first time it's seen, reset to 0 per scene by ResetEntityDisplayIds (see
// main.cpp's File > New handler) -- both GetEntityLabel's numbering and
// DrawEntityListPanel's ordering are built on this one shared source.
std::unordered_map<asge::ecs::Entity, std::uint32_t> g_EntityDisplayIds;
std::uint32_t g_NextEntityDisplayId = 0;

std::uint32_t GetOrAssignDisplayId( asge::ecs::Entity inEntity ) noexcept
{
    auto const [it, inserted] = g_EntityDisplayIds.try_emplace( inEntity, g_NextEntityDisplayId );
    if ( inserted ) ++g_NextEntityDisplayId;
    return it->second;
}

/**
 * @brief ImGuiCond_Always exactly on the frame DisplaySize actually
 *        changed (a live resize), ImGuiCond_FirstUseEver every other frame
 *        -- lets a right-edge-anchored panel re-snap to the edge on resize
 *        while staying freely user-draggable the rest of the time. Forcing
 *        Always unconditionally (the first attempt at this anchoring)
 *        re-fought the user's own drag every single frame, making the
 *        panel effectively immovable.
 *
 * Guarded by GetFrameCount() rather than recomputing on every call, since
 * both DrawEntityListPanel and DrawInspectorPanel call this in the same
 * frame -- without the guard, the second call would compare DisplaySize
 * against what the first call just stored (this same frame, unchanged),
 * always reporting "not resized" regardless of whether it actually was.
 */
ImGuiCond AnchorCondOnResize() noexcept
{
    static ImVec2 s_LastDisplaySize{ 0.0f, 0.0f };
    static int s_LastCheckedFrame = -1;
    static bool s_ResizedThisFrame = false;

    int const frame = ImGui::GetFrameCount();
    if ( frame != s_LastCheckedFrame )
    {
        ImVec2 const displaySize = ImGui::GetIO().DisplaySize;
        s_ResizedThisFrame = displaySize.x != s_LastDisplaySize.x || displaySize.y != s_LastDisplaySize.y;
        s_LastDisplaySize = displaySize;
        s_LastCheckedFrame = frame;
    }
    return s_ResizedThisFrame ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
}

// Edits a std::string field through a scratch fixed-size buffer, re-synced
// from inValue every call -- the standard immediate-mode pattern for
// InputText, which needs a char* rather than a std::string.
bool DrawTextField( char const* inLabel, std::string& ioValue ) noexcept
{
    char buf[256];
    std::snprintf( buf, sizeof(buf), "%s", ioValue.c_str() );
    if ( ImGui::InputText( inLabel, buf, sizeof(buf) ) )
    {
        ioValue = buf;
        return true;
    }
    return false;
}

// Phase 11: every DrawInspector now returns bool (true if it changed
// anything) -- not for the ResolveAssets trigger DrawSection<T>'s bool
// detection originally existed for (that stays scoped to just Sprite/
// Animation/AudioSource's own path-changed signal below, unchanged), but as
// a second, independent signal DrawInspectorPanel ORs separately into
// whether the active scene should be marked unsaved. Deliberately not the
// same signal as componentsChanged: a continuously-dragged field (Position,
// Zoom, ...) fires every frame it's held, and ResolveAssets isn't safe to
// call that often -- a still-broken asset's resolve failure would get
// re-logged every one of those frames instead of once (the exact bug Phase
// 7 already fixed by narrowing ResolveAssets' trigger in the first place).
bool DrawInspector( Name& inName ) noexcept
{
    return DrawTextField( "Name", inName.m_Name );
}

bool DrawInspector( Transform& inT ) noexcept
{
    bool changed = false;

    if ( ImGui::Checkbox( "Locked", &inT.m_Locked ) ) changed = true;

    // A locked entity can't be moved from here either, same as in the viewport.
    ImGui::BeginDisabled( inT.m_Locked );
    float pos[2]{ inT.m_LocalCoordinates.x(), inT.m_LocalCoordinates.y() };
    if ( ImGui::DragFloat2( "Position", pos ) )
    {
        inT.m_LocalCoordinates = { pos[0], pos[1] };
        inT.m_Dirty = true;
        changed = true;
    }
    ImGui::EndDisabled();

    if ( ImGui::DragFloat( "Rotation (rad)", &inT.m_LocalRotation, 0.01f ) ) { inT.m_Dirty = true; changed = true; }

    float scale[2]{ inT.m_LocalScale.x(), inT.m_LocalScale.y() };
    if ( ImGui::DragFloat2( "Scale", scale ) )
    {
        inT.m_LocalScale = { scale[0], scale[1] };
        inT.m_Dirty = true;
        changed = true;
    }

    return changed;
}

bool DrawInspector( Velocity& inVelocity ) noexcept
{
    float vel[2]{ inVelocity.m_DX, inVelocity.m_DY };
    if ( ImGui::DragFloat2( "Velocity", vel ) ) { inVelocity.m_DX = vel[0]; inVelocity.m_DY = vel[1]; return true; }
    return false;
}

bool DrawInspector( Rigidbody& inRigidbody ) noexcept
{
    bool changed = ImGui::DragFloat( "Mass", &inRigidbody.m_Mass, 0.1f, 0.0f, 1000.0f );
    if ( ImGui::Checkbox( "Affected By Gravity", &inRigidbody.m_AffectedByGravity ) ) changed = true;
    return changed;
}

// m_SourceRect editing is out of scope here (asset-browsing/viewport gizmo
// territory). m_VirtualPath is a dropdown restricted to inKnownTextures
// (Phase 10) -- its return reports only whether the path selection changed,
// since only a path change needs AssetManager::ResolveAssets re-run (see the
// DrawInspector doc comment above for why that trigger has to stay this
// narrow). Draw order (layer/y-sort/screen-space) moved out to its own
// components::RenderInfo section, below.
bool DrawInspector( Sprite& inSprite, std::vector<std::string> const& inKnownTextures ) noexcept
{
    return DrawAssetPathCombo( "Virtual Path", inSprite.m_VirtualPath, inKnownTextures );
}

// Phase 14: every field round-trips through Serializer<RenderInfo> verbatim
// (no asset path/entity reference to reconcile), so unlike Sprite's own
// section every edit here can just report "changed" directly.
//
// An entity that inherits its sort from a parent draws with that parent's
// resolved layer and y-sort, so its own are ignored (RenderSystem's
// ResolveRenderInfo) -- but only if it actually has a parent, which is what
// inHasParent says. Both fields are greyed out then, rather than left
// looking editable while doing nothing.
bool DrawInspector( RenderInfo& inRenderInfo, bool inHasParent ) noexcept
{
    bool const inherits = inRenderInfo.m_InheritSortFromParent && inHasParent;

    ImGui::BeginDisabled( inherits );
    bool changed = ImGui::DragInt( "Layer", &inRenderInfo.m_Layer );
    if ( ImGui::Checkbox( "Y-Sort", &inRenderInfo.m_YSort ) ) changed = true;
    ImGui::EndDisabled();

    if ( ImGui::Checkbox( "Screen Space", &inRenderInfo.m_ScreenSpace ) ) changed = true;
    if ( ImGui::Checkbox( "Inherit Sort From Parent", &inRenderInfo.m_InheritSortFromParent ) ) changed = true;
    if ( inherits ) ImGui::TextDisabled( "Layer and Y-Sort come from the parent." );
    if ( ImGui::DragInt( "Local Order", &inRenderInfo.m_LocalOrder ) ) changed = true;
    if ( ImGui::DragFloat( "Sort Offset Y", &inRenderInfo.m_SortOffsetY ) ) changed = true;
    return changed;
}

// Per-entity cache of each shape's own last-seen dimensions, so switching
// Rect -> Circle -> Rect restores what was there before rather than always
// resetting to a fresh default (Phase 12 step 4). Never pruned, same as
// g_EntityDisplayIds above -- a stale entry for a since-deleted entity just
// sits unused.
std::unordered_map<asge::ecs::Entity, std::pair<asge::math::Rect, asge::math::Circle>> g_ColliderShapeCache;

bool DrawInspector( Collider& inCollider, asge::ecs::Entity inEntity, ColliderDrawState& ioColliderDraw ) noexcept
{
    bool changed = false;
    auto& [cachedRect, cachedCircle] = g_ColliderShapeCache[inEntity];

    // Mirrors whatever's currently live every frame -- picks up edits made
    // either by the fields below or by the "Draw Collider" viewport mode
    // (main.cpp mutates inCollider.m_LocalBounds directly, not through
    // here), so a later shape switch always restores the true last state.
    if ( auto* rect = std::get_if<asge::math::Rect>( &inCollider.m_LocalBounds ) ) cachedRect = *rect;
    else if ( auto* circle = std::get_if<asge::math::Circle>( &inCollider.m_LocalBounds ) ) cachedCircle = *circle;

    static char const* const kShapeNames[]{ "Rect", "Circle" };
    int shapeIndex = static_cast<int>( inCollider.m_LocalBounds.index() );
    if ( ImGui::Combo( "Collider Shape", &shapeIndex, kShapeNames, 2 ) )
    {
        inCollider.m_LocalBounds = shapeIndex == 0
            ? ColliderShape{ cachedRect }
            : ColliderShape{ cachedCircle };
        changed = true;
    }

    if ( auto* rect = std::get_if<asge::math::Rect>( &inCollider.m_LocalBounds ) )
    {
        float vals[4]{ rect->m_X, rect->m_Y, rect->m_Width, rect->m_Height };
        if ( ImGui::DragFloat4( "Rect (x,y,w,h)", vals ) )
        {
            rect->m_X = vals[0]; rect->m_Y = vals[1]; rect->m_Width = vals[2]; rect->m_Height = vals[3];
            changed = true;
        }
    }
    else if ( auto* circle = std::get_if<asge::math::Circle>( &inCollider.m_LocalBounds ) )
    {
        float center[2]{ circle->m_Center.x(), circle->m_Center.y() };
        if ( ImGui::DragFloat2( "Circle Center", center ) )
        {
            circle->m_Center = asge::math::Float2{ center[0], center[1] };
            changed = true;
        }
        if ( ImGui::DragFloat( "Radius", &circle->m_Radius ) ) changed = true;
    }

    bool const drawingThis = ioColliderDraw.m_Active && ioColliderDraw.m_Entity == inEntity;
    if ( drawingThis )
    {
        ImGui::TextDisabled( "Drag on the viewport to define the shape; ESC to cancel." );
    }
    else
    {
        if ( ImGui::Button( "Draw Collider" ) )
        {
            ioColliderDraw.m_Active = true;
            ioColliderDraw.m_Entity = inEntity;
            ioColliderDraw.m_Snapshot = inCollider.m_LocalBounds;
        }
        ImGui::SameLine();
        if ( ImGui::Button( "Reset Collider" ) )
        {
            if ( auto* rect = std::get_if<asge::math::Rect>( &inCollider.m_LocalBounds ) ) *rect = asge::math::Rect{};
            else if ( auto* circle = std::get_if<asge::math::Circle>( &inCollider.m_LocalBounds ) ) *circle = asge::math::Circle{};
            changed = true;
        }
    }

    static char const* const kResolutionNames[]{ "Unknown", "Trigger", "Solid" };
    int resIndex = static_cast<int>( inCollider.m_Resolution );
    if ( ImGui::Combo( "Resolution", &resIndex, kResolutionNames, 3 ) )
    {
        inCollider.m_Resolution = static_cast<ResolutionType>( resIndex );
        changed = true;
    }

    if ( ImGui::InputScalar( "Layer", ImGuiDataType_U32, &inCollider.m_Layer ) ) changed = true;
    if ( ImGui::InputScalar( "Mask", ImGuiDataType_U32, &inCollider.m_Mask ) ) changed = true;

    return changed;
}

bool DrawInspector( Camera& inCamera ) noexcept
{
    bool changed = ImGui::DragFloat( "Zoom", &inCamera.m_Zoom, 0.01f, 0.01f, 100.0f );
    if ( ImGui::DragFloat( "Smoothing", &inCamera.m_Smoothing, 0.01f, 0.0f, 100.0f ) ) changed = true;
    return changed;
}

// Only m_VirtualClipPath round-trips through save (see Serializer<AudioSource>'s
// doc comment) -- playback state is runtime-only, so it isn't exposed here.
// m_VirtualClipPath is a dropdown restricted to inKnownAudio (Phase 10); the
// return reports whether that selection changed.
bool DrawInspector( AudioSource& inAudioSource, std::vector<std::string> const& inKnownAudio ) noexcept
{
    return DrawAssetPathCombo( "Clip Path", inAudioSource.m_VirtualClipPath, inKnownAudio );
}

// Only m_ClipPath/m_FrameDuration round-trip (see Serializer<Animation>'s
// doc comment) -- playback progress is runtime-only. m_ClipPath is a
// dropdown restricted to inKnownAnimations (Phase 10); the return reports
// only whether that selection changed, not a m_FrameDuration edit, since
// only a clip change needs AssetManager::ResolveAssets re-run (same
// narrow-trigger reasoning as Sprite's own DrawInspector above).
// ponytail: same gap as Sprite's -- a m_FrameDuration-only edit doesn't
// mark the scene dirty on its own.
bool DrawInspector( Animation& inAnimation, std::vector<std::string> const& inKnownAnimations ) noexcept
{
    bool const clipChanged = DrawAssetPathCombo( "Clip Path", inAnimation.m_ClipPath, inKnownAnimations );
    ImGui::DragFloat( "Frame Duration", &inAnimation.m_FrameDuration, 0.01f, 0.0f, 10.0f );

    // Added components start stopped (see MakeComponentEntry's Add lambda
    // above), and every scene load/switch stops them too (see
    // SwitchToScene) -- this button is the only remaining way to turn one
    // on. Purely a live, runtime m_Playing flip for previewing, same as
    // PlayAnimation/StopAnimation always were; nothing here round-trips
    // through TOML. Label/action flips with the live state so a currently-
    // playing clip shows "Stop", not a stale "Play".
    if ( inAnimation.m_Playing )
    {
        if ( ImGui::Button( "Stop" ) ) StopAnimation( inAnimation );
    }
    else
    {
        if ( ImGui::Button( "Play" ) ) PlayAnimation( inAnimation );
    }

    return clipChanged;
}

// Waypoints are authored by clicking the viewport (Phase 12's "Select
// Waypoints" mode), not typed here -- see main.cpp's WaypointEditState
// handling for the actual click-to-add/ESC-to-cancel logic and the overlay
// that shows points as they're placed. This just owns the toggle button and
// the snapshot taken when entering the mode, so cancelling can restore it.
bool DrawInspector( PathFollow& inPathFollow, asge::ecs::Entity inEntity, WaypointEditState& ioWaypointEdit ) noexcept
{
    ImGui::Text( "Waypoints: %zu", inPathFollow.m_Waypoints.size() );

    bool const editingThis = ioWaypointEdit.m_Active && ioWaypointEdit.m_Entity == inEntity;
    if ( ImGui::Button( editingThis ? "End Selection" : "Select Waypoints" ) )
    {
        if ( editingThis )
        {
            ioWaypointEdit = WaypointEditState{}; // commit -- keep whatever was placed
        }
        else
        {
            ioWaypointEdit.m_Active = true;
            ioWaypointEdit.m_Entity = inEntity;
            ioWaypointEdit.m_Snapshot = inPathFollow.m_Waypoints;
        }
    }
    if ( editingThis )
    {
        ImGui::SameLine();
        ImGui::TextDisabled( "(click the viewport to add; ESC to cancel)" );
    }

    bool changed = ImGui::DragFloat( "Speed", &inPathFollow.m_Speed );
    if ( ImGui::Checkbox( "Loop", &inPathFollow.m_Loop ) ) changed = true;

    int resolution = static_cast<int>( inPathFollow.m_Resolution );
    if ( ImGui::DragInt( "Resolution", &resolution, 1.0f, 1, 256 ) )
    {
        inPathFollow.m_Resolution = static_cast<std::size_t>( resolution );
        RebuildPath( inPathFollow ); // m_Path's sampling density depends on this, unlike Speed/Loop
        changed = true;
    }
    return changed;
}

// ImGui::ColorEdit4 works in float[4] (0..1); RGBA_Color is uint8 (0..255) --
// converts both ways, same round-trip shape DrawTextField's scratch buffer
// is for std::string/InputText.
bool DrawColorField( char const* inLabel, asge::graphics::RGBA_Color& ioColor ) noexcept
{
    float rgba[4]{ ioColor.r / 255.0f, ioColor.g / 255.0f, ioColor.b / 255.0f, ioColor.a / 255.0f };
    if ( !ImGui::ColorEdit4( inLabel, rgba ) ) return false;

    auto const toU8 = []( float inV ) noexcept
    { return static_cast<std::uint8_t>( std::clamp( inV, 0.0f, 1.0f ) * 255.0f + 0.5f ); };
    ioColor = { toU8( rgba[0] ), toU8( rgba[1] ), toU8( rgba[2] ), toU8( rgba[3] ) };
    return true;
}

// Phase 16: UIRect/Interactable/UIButton/UILabel are never in kComponentEntries
// (see DrawSection's own "no entry -> no remove button" fallback) -- a UI
// widget is a bundle CreateButton/CreateLabel assembles together (see
// src/ASGE/Game/UI.hpp), not something addable/removable component-by-
// component; deleting the whole entity is how one goes away.
// inEntity/inRegistry are only for checking a sibling UILabel's m_AutoSize
// (Phase 16) -- RenderSystem overwrites m_Size from the label's own measured
// text every frame while that's set (see RenderSystem.hpp's own doc
// comment), so editing it here would just get silently stomped right back.
bool DrawInspector( UIRect& inRect, asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
{
    bool const autoSized = [&]
    {
        auto label = inRegistry.GetComponent<UILabel>( inEntity );
        return label && label.Value().get().m_AutoSize;
    }();

    if ( autoSized )
    {
        ImGui::BeginDisabled();
        float size[2]{ inRect.m_Size.x(), inRect.m_Size.y() };
        ImGui::DragFloat2( "Size", size );
        ImGui::EndDisabled();
        ImGui::TextDisabled( "(sized automatically -- see UILabel's Auto Size)" );
        return false;
    }

    float size[2]{ inRect.m_Size.x(), inRect.m_Size.y() };
    if ( !ImGui::DragFloat2( "Size", size, 1.0f, 1.0f, 4096.0f ) ) return false;
    inRect.m_Size = { size[0], size[1] };
    return true;
}

// m_Hovered/m_Held/m_Clicked are runtime-only (systems::UIInteractionSystem
// recomputes them every frame -- see Interactable's own doc comment), so
// only m_Enabled is exposed here.
bool DrawInspector( Interactable& inInteractable ) noexcept
{
    return ImGui::Checkbox( "Enabled", &inInteractable.m_Enabled );
}

// m_OnClick is a runtime signal, not something a scene file describes --
// nothing here to expose.
bool DrawInspector( UIButton& inButton ) noexcept
{
    bool changed = DrawColorField( "Color", inButton.m_Colors.m_Color );
    if ( DrawColorField( "Hover Color", inButton.m_Colors.m_HoverColor ) ) changed = true;
    if ( DrawColorField( "Pressed Color", inButton.m_Colors.m_PressedColor ) ) changed = true;
    return changed;
}

// m_FontPath is a dropdown restricted to inKnownFonts (Phase 16, same
// "picked from what's known to be loaded" treatment Sprite/Animation/
// AudioSource's own paths already get) -- the return reports only whether
// that selection changed, since only a font change needs
// AssetManager::ResolveAssets re-run (same narrow-trigger reasoning as
// Sprite's own DrawInspector above).
bool DrawInspector( UILabel& inLabel, std::vector<std::string> const& inKnownFonts ) noexcept
{
    bool changed = DrawAssetPathCombo( "Font Path", inLabel.m_FontPath, inKnownFonts );

    DrawTextField( "Text", inLabel.m_Text );

    static char const* const kAlignNames[]{ "None", "Left", "Center", "Right" };
    int align = static_cast<int>( inLabel.m_Align );
    if ( ImGui::Combo( "Align", &align, kAlignNames, 4 ) )
    {
        inLabel.m_Align = static_cast<asge::str::TextAlign>( align );
    }

    static char const* const kVAlignNames[]{ "Top", "Center", "Bottom" };
    int valign = static_cast<int>( inLabel.m_VerticalAlign );
    if ( ImGui::Combo( "Vertical Align", &valign, kVAlignNames, 3 ) )
    {
        inLabel.m_VerticalAlign = static_cast<VerticalAlign>( valign );
    }

    DrawColorField( "Color", inLabel.m_Color );
    // A pixel-height change needs the same re-resolve a path change does --
    // AssetManager::GetFont caches by (path, pixel height), so this is a
    // different baked Font asset, not just a bigger/smaller draw of the same
    // one (see Resolver<UILabel>'s own doc comment).
    if ( ImGui::DragInt( "Font Size", &inLabel.m_FontPixelHeight, 1.0f, 1, 256 ) ) changed = true;
    if ( ImGui::IsItemHovered() )
    {
        auto const atlasSize = asge::media::Font::GetAtlasSize();
        ImGui::SetTooltip(
            "Every font bakes into a fixed %dx%d atlas -- too large a size for a "
            "given font's own glyphs to all fit fails to resolve.", atlasSize.x(), atlasSize.y() );
    }
    ImGui::Checkbox( "Auto Size", &inLabel.m_AutoSize );
    ImGui::BeginDisabled( inLabel.m_AutoSize );
    ImGui::Checkbox( "Word Wrap", &inLabel.m_WordWrap );
    ImGui::EndDisabled();

    return changed;
}

// Phase 17: UICheckbox/UISlider/UIPanel, like the Phase 16 widgets, are view/
// edit-only (assembled by CreateCheckbox/CreateSlider/CreatePanel, never added
// piecemeal). m_OnToggled/m_OnValueChanged are runtime signals -- nothing to
// expose. m_Checked/m_Value are edited directly so the viewport shows them.
bool DrawInspector( UICheckbox& inCheckbox ) noexcept
{
    bool changed = ImGui::Checkbox( "Checked", &inCheckbox.m_Checked );
    if ( DrawColorField( "Color", inCheckbox.m_BoxColors.m_Color ) ) changed = true;
    if ( DrawColorField( "Hover Color", inCheckbox.m_BoxColors.m_HoverColor ) ) changed = true;
    if ( DrawColorField( "Pressed Color", inCheckbox.m_BoxColors.m_PressedColor ) ) changed = true;
    if ( DrawColorField( "Check Color", inCheckbox.m_CheckColor ) ) changed = true;
    return changed;
}

bool DrawInspector( UISlider& inSlider ) noexcept
{
    bool changed = ImGui::DragFloat( "Min", &inSlider.m_Min, 0.05f );
    if ( ImGui::DragFloat( "Max", &inSlider.m_Max, 0.05f ) ) changed = true;
    // A value outside [Min, Max] just draws pinned to the nearer end (see
    // GetThumbPosition), but clamping here keeps what's saved honest.
    if ( ImGui::SliderFloat( "Value", &inSlider.m_Value, inSlider.m_Min, inSlider.m_Max ) ) changed = true;
    if ( DrawColorField( "Track Color", inSlider.m_TrackColor ) ) changed = true;
    if ( DrawColorField( "Thumb Color", inSlider.m_ThumbColor.m_Color ) ) changed = true;
    if ( DrawColorField( "Thumb Hover Color", inSlider.m_ThumbColor.m_HoverColor ) ) changed = true;
    if ( DrawColorField( "Thumb Pressed Color", inSlider.m_ThumbColor.m_PressedColor ) ) changed = true;
    return changed;
}

// Switching the layout type replaces m_Layout's whole variant alternative
// (rows/cols reset to that type's defaults); Grid/VStack/HStack then expose
// their own row/column counts. The panel lays out its Hierarchy children (see
// systems::UILayoutSystem), so any child of a non-Absolute panel has its
// position (and, unless its UILayoutItem says otherwise, size) re-derived
// every frame -- editing those on the child itself is overwritten right back.
bool DrawInspector( UIPanel& inPanel ) noexcept
{
    bool changed = DrawColorField( "Background", inPanel.m_Background );
    if ( ImGui::Checkbox( "Border", &inPanel.m_Border ) ) changed = true;
    if ( inPanel.m_Border && DrawColorField( "Border Color", inPanel.m_BorderColor ) ) changed = true;

    auto const drawFloat2 = [&]( char const* inLabel, asge::math::Float2& ioValue ) noexcept
    {
        float v[2]{ ioValue.x(), ioValue.y() };
        if ( !ImGui::DragFloat2( inLabel, v, 0.5f, 0.0f, 4096.0f ) ) return;
        ioValue = { v[0], v[1] };
        changed = true;
    };
    drawFloat2( "Margin", inPanel.m_Margin );
    drawFloat2( "Padding", inPanel.m_Padding );
    drawFloat2( "Spacing", inPanel.m_Spacing );

    static char const* const kLayoutNames[]{ "Absolute", "Grid", "VStack", "HStack" };
    int layout = static_cast<int>( GetPanelLayout( inPanel ) );
    if ( ImGui::Combo( "Layout", &layout, kLayoutNames, 4 ) )
    {
        switch ( static_cast<PanelLayout>( layout ) )
        {
        case PanelLayout::Absolute: inPanel.m_Layout = LayoutAbsolute{}; break;
        case PanelLayout::Grid:     inPanel.m_Layout = LayoutGrid{}; break;
        case PanelLayout::VStack:   inPanel.m_Layout = LayoutVStack{}; break;
        case PanelLayout::HStack:   inPanel.m_Layout = LayoutHStack{}; break;
        }
        changed = true;
    }

    if ( auto* grid = std::get_if<LayoutGrid>( &inPanel.m_Layout ) )
    {
        if ( ImGui::DragInt( "Rows", &grid->m_Rows, 0.1f, 1, 64 ) ) changed = true;
        if ( ImGui::DragInt( "Columns", &grid->m_Cols, 0.1f, 1, 64 ) ) changed = true;
    }
    else if ( auto* vstack = std::get_if<LayoutVStack>( &inPanel.m_Layout ) )
    {
        if ( ImGui::DragInt( "Rows", &vstack->m_Rows, 0.1f, 1, 64 ) ) changed = true;
    }
    else if ( auto* hstack = std::get_if<LayoutHStack>( &inPanel.m_Layout ) )
    {
        if ( ImGui::DragInt( "Columns", &hstack->m_Cols, 0.1f, 1, 64 ) ) changed = true;
    }

    return changed;
}

// Only meaningful under a UIPanel parent with a non-Absolute layout, but
// addable to any entity (a child can be attached to a panel afterwards).
bool DrawInspector( UILayoutItem& inItem ) noexcept
{
    static char const* const kAlignNames[]{ "Start", "Center", "End" };

    bool changed = ImGui::Checkbox( "Fill X", &inItem.m_FillX );
    if ( ImGui::Checkbox( "Fill Y", &inItem.m_FillY ) ) changed = true;

    int alignX = static_cast<int>( inItem.m_AlignX ), alignY = static_cast<int>( inItem.m_AlignY );
    if ( ImGui::Combo( "Align X", &alignX, kAlignNames, 3 ) )
    {
        inItem.m_AlignX = static_cast<SlotAlign>( alignX );
        changed = true;
    }
    if ( ImGui::Combo( "Align Y", &alignY, kAlignNames, 3 ) )
    {
        inItem.m_AlignY = static_cast<SlotAlign>( alignY );
        changed = true;
    }
    return changed;
}

// One entry per DrawSection<T> call below -- reused to drive "Add
// Component"'s list, since the set of addable types is exactly the set of
// drawable types. Function pointers (not std::function) since every
// closure is capture-less; T is erased here so the array can hold every
// type uniformly.
struct ComponentEntry
{
    char const* m_Name;
    bool (*m_Has)( asge::ecs::Registry&, asge::ecs::Entity ) noexcept;
    void (*m_Add)( asge::ecs::Registry&, asge::ecs::Entity ) noexcept;
    void (*m_Remove)( asge::ecs::Registry&, asge::ecs::Entity ) noexcept;
};

template<typename T>
constexpr ComponentEntry MakeComponentEntry( char const* inName ) noexcept
{
    return ComponentEntry{
        inName,
        []( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
        { return inRegistry.HasComponent<T>( inEntity ); },
        []( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
        {
            T component{};
            // A freshly-added Animation shouldn't start cycling frames in
            // the editor's own viewport (RenderPipeline runs AnimationSystem
            // every frame) the moment a clip path is picked -- AudioSource
            // already defaults m_Playing to false for the same reason;
            // Animation's own in-code default (true) is right for a game
            // spawning one ready-to-go, just not for this editor action.
            if constexpr ( std::is_same_v<T, Animation> ) StopAnimation( component );
            inRegistry.AddComponent<T>( inEntity, component );
        },
        []( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
        {
            // AnimationSystem writes each cropped frame straight into the
            // Sprite's own m_SourceRect (RenderSystem.cpp) -- removing
            // Animation doesn't touch that, so without this the Sprite is
            // left showing whatever frame it was last cropped to instead of
            // going back to the whole spritesheet (m_SourceRect == nullopt).
            if constexpr ( std::is_same_v<T, Animation> )
            {
                if ( auto sprite = inRegistry.GetComponent<Sprite>( inEntity ) )
                {
                    sprite.Value().get().m_SourceRect = std::nullopt;
                }
            }
            if ( auto const result = inRegistry.RemoveComponent<T>( inEntity ); !result ) result.LogError();
        }
    };
}

ComponentEntry const kComponentEntries[]{
    MakeComponentEntry<Name>( "Name" ),
    MakeComponentEntry<Transform>( "Transform" ),
    MakeComponentEntry<Velocity>( "Velocity" ),
    MakeComponentEntry<Rigidbody>( "Rigidbody" ),
    MakeComponentEntry<Sprite>( "Sprite" ),
    MakeComponentEntry<RenderInfo>( "RenderInfo" ),
    MakeComponentEntry<Collider>( "Collider" ),
    MakeComponentEntry<Camera>( "Camera" ),
    MakeComponentEntry<AudioSource>( "AudioSource" ),
    MakeComponentEntry<Animation>( "Animation" ),
    MakeComponentEntry<PathFollow>( "PathFollow" ),
    MakeComponentEntry<UILayoutItem>( "UILayoutItem" ),
};

// Linear scan over kComponentEntries by name -- only ever called once per
// DrawSection<T> per frame (10 entries, 10 sections), not worth a map.
ComponentEntry const* FindComponentEntry( char const* inName ) noexcept
{
    for ( auto const& entry : kComponentEntries )
    {
        if ( std::strcmp( entry.m_Name, inName ) == 0 ) return &entry;
    }
    return nullptr;
}

// Combo of component types inSelected doesn't already have; picking one and
// hitting "Add" default-constructs it onto the entity. Combo selection index
// is a function-local static -- fine here since only one Inspector panel is
// ever drawn at a time (same convention Registry's own internals use).
// Sprite is special-cased: it's the one type whose whole point is naming an
// asset, so it also requires picking one of inKnownTextures up front rather
// than defaulting to a blank, unresolved m_VirtualPath -- see
// editor/AssetBrowser.hpp's KnownTexturePaths for where that list comes from.
// @return True the one frame "Add Component" was actually clicked.
bool DrawAddComponentControl(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity, std::vector<std::string> const& inKnownTextures ) noexcept
{
    char const* missingNames[std::size( kComponentEntries )];
    ComponentEntry const* missingEntries[std::size( kComponentEntries )];
    int missingCount = 0;
    for ( auto const& entry : kComponentEntries )
    {
        if ( !entry.m_Has( inRegistry, inEntity ) )
        {
            missingNames[missingCount] = entry.m_Name;
            missingEntries[missingCount] = &entry;
            ++missingCount;
        }
    }

    if ( missingCount == 0 ) return false;

    static int selected = 0;
    if ( selected >= missingCount ) selected = 0;

    ImGui::Separator();
    ImGui::SetNextItemWidth( 150.0f );
    ImGui::Combo( "##AddComponentType", &selected, missingNames, missingCount );

    if ( std::strcmp( missingNames[selected], "Sprite" ) == 0 )
    {
        if ( inKnownTextures.empty() )
        {
            ImGui::TextDisabled( "No textures loaded yet -- use Load Asset in the Assets panel." );
            return false;
        }

        static int textureIndex = 0;
        if ( textureIndex >= static_cast<int>( inKnownTextures.size() ) ) textureIndex = 0;

        std::vector<char const*> textureNames;
        textureNames.reserve( inKnownTextures.size() );
        for ( auto const& path : inKnownTextures ) textureNames.push_back( path.c_str() );

        ImGui::SetNextItemWidth( 150.0f );
        ImGui::Combo( "##SpriteTexture", &textureIndex, textureNames.data(), static_cast<int>( textureNames.size() ) );
        ImGui::SameLine();
        if ( ImGui::Button( "Add Component" ) )
        {
            Sprite sprite{};
            sprite.m_VirtualPath = inKnownTextures[textureIndex];
            inRegistry.AddComponent<Sprite>( inEntity, sprite );
            (void)inRegistry.GetOrAddComponent<RenderInfo>( inEntity ); // Phase 14: every Sprite gets a RenderInfo, once
            return true;
        }
        return false;
    }

    ImGui::SameLine();
    if ( ImGui::Button( "Add Component" ) )
    {
        missingEntries[selected]->m_Add( inRegistry, inEntity );
        return true;
    }
    return false;
}

// Draws inName's section (if inEntity has a T) under its own ID scope, so
// two component types that happen to use the same field label (e.g. Sprite
// and Collider both have a "Layer") don't collide onto the same ImGui ID
// within the shared Inspector window -- SeparatorText is a label, not an ID
// scope, so without this every widget past the first "Layer" field would
// silently share state with it. The trailing "x" button removes T via
// kComponentEntries' erased m_Remove, looked up by inName rather than
// threading a second template through the call site.
//
// inExtra is forwarded straight to DrawInspector, so a type needing more
// context than just its own component (Sprite/Animation/AudioSource's known-
// paths list, Phase 10) can take it as an extra parameter without this
// template itself knowing anything about that -- overload resolution alone
// picks the right DrawInspector. Whether DrawInspector's return is bool
// (a path-selection change that should trigger ComponentsChanged) or void
// (nothing here needs re-resolving) is likewise detected via decltype
// rather than this template hardcoding which types report one.
// @return True the one frame the "x" button removed T, or DrawInspector
//         reported a change worth re-resolving assets for.
template<typename T, typename... Extra>
bool DrawSection( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity, char const* inName, Extra&&... inExtra ) noexcept
{
    if ( auto r = inRegistry.GetComponent<T>( inEntity ) )
    {
        ImGui::PushID( inName );
        ImGui::SeparatorText( inName );

        // No entry -- a Phase 16 UI component type, never addable/removable
        // through the generic control (see this template's own doc comment
        // above) -- means no "x" at all, rather than dereferencing a null
        // ComponentEntry*.
        if ( ComponentEntry const* entry = FindComponentEntry( inName ) )
        {
            ImGui::SameLine( ImGui::GetWindowWidth() - 30.0f );
            if ( ImGui::SmallButton( "x" ) )
            {
                entry->m_Remove( inRegistry, inEntity );
                ImGui::PopID();
                return true;
            }
        }

        bool changed = false;
        if constexpr ( std::is_same_v<decltype( DrawInspector( r.Value().get(), std::forward<Extra>( inExtra )... ) ), bool> )
        {
            changed = DrawInspector( r.Value().get(), std::forward<Extra>( inExtra )... );
        }
        else
        {
            DrawInspector( r.Value().get(), std::forward<Extra>( inExtra )... );
        }

        ImGui::PopID();
        return changed;
    }
    return false;
}

// Phase 15: true if inEntity itself carries the Disable marker, or any
// entity anywhere in its own subtree does -- independent of whether that
// subtree is currently expanded in the tree, so a collapsed ancestor's row
// still shows the closed-eye hint below without having to open every level
// down to the actually-disabled entity.
bool SubtreeHasDisabled( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
{
    if ( inRegistry.HasComponent<asge::ecs::markers::Disable>( inEntity ) ) return true;

    bool found = false;
    asge::ecs::components::ForEachChild( inRegistry, inEntity, [&]( asge::ecs::Entity inChild )
    {
        if ( !found && SubtreeHasDisabled( inRegistry, inChild ) ) found = true;
    } );
    return found;
}

// A small hand-drawn closed-eye glyph -- no icon font in this project (same
// reasoning as AssetInspector.cpp's transport-control icons): a shallow
// eyelid arc plus two short lashes, distinct enough from an open eye
// (which would have a pupil) at this size. Purely a paint call, not a
// widget -- callers place it themselves, same technique
// PlayPauseIconButton/RewindIconButton use for their own icon body.
void DrawClosedEyeIcon( ImDrawList* inDrawList, ImVec2 inCenter, float inRadius, ImU32 inColor ) noexcept
{
    ImVec2 const left{ inCenter.x - inRadius, inCenter.y };
    ImVec2 const right{ inCenter.x + inRadius, inCenter.y };
    ImVec2 const top{ inCenter.x, inCenter.y - inRadius * 0.6f };
    inDrawList->AddBezierQuadratic( left, top, right, inColor, 1.5f );

    inDrawList->AddLine( left,  ImVec2{ left.x - inRadius * 0.35f,  left.y + inRadius * 0.5f },  inColor, 1.5f );
    inDrawList->AddLine( right, ImVec2{ right.x + inRadius * 0.35f, right.y + inRadius * 0.5f }, inColor, 1.5f );
}

// What a click in the Entities panel asked of the groups this frame. Applied
// after the whole tree is drawn, never mid-walk, since drawing iterates the
// very lists an operation would change.
enum class GroupOpKind { None, NewEmpty, NewWith, MoveTo, RemoveFrom, Delete, Rename };

struct GroupOp
{
    GroupOpKind       m_Kind   = GroupOpKind::None;
    std::size_t       m_Group  = 0;
    asge::ecs::Entity m_Entity = asge::ecs::Entity::Null();
};

struct GroupContext
{
    EntityGroupList&       m_Groups;
    GroupOp                m_Op;
    bool                   m_PromptForName = false; // m_Op needs a name from the Group Name modal first
};

constexpr std::size_t kNoGroup = static_cast<std::size_t>( -1 );

std::size_t FindGroupOf( EntityGroupList const& inGroups, asge::ecs::Entity inEntity ) noexcept
{
    for ( std::size_t i = 0; i < inGroups.size(); ++i )
    {
        auto const& members = inGroups[i].m_Members;
        if ( std::find( members.begin(), members.end(), inEntity ) != members.end() ) return i;
    }
    return kNoGroup;
}

void RemoveFromAllGroups( EntityGroupList& ioGroups, asge::ecs::Entity inEntity ) noexcept
{
    for ( auto& group : ioGroups ) std::erase( group.m_Members, inEntity );
}

// inEntity's parent, or Null() for a top-level entity -- the scope a group it joins must have.
asge::ecs::Entity ParentOf( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
{
    auto const hierarchy = inRegistry.GetComponent<asge::ecs::components::Hierarchy>( inEntity );
    return hierarchy ? hierarchy.Value().get().m_Parent : asge::ecs::Entity::Null();
}

// Draws the groups whose parent is inScope (Null() for the top level), then inChildren that no
// group holds. Defined after DrawGroupNode, which draws a group's members through DrawEntityTreeNode.
void DrawScope(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inScope, std::vector<asge::ecs::Entity> const& inChildren,
    asge::ecs::Entity& ioSelected, EntityListResult& ioResult, GroupContext& ioGroups ) noexcept;

// Phase 13: one row of DrawEntityListPanel's tree, recursing into
// inEntity's own children (if any) via ecs::components::ForEachChild.
// Reports at most one HierarchyAction into ioResult per frame -- New Child/
// Detach/Remove from this row's own context menu, or Reparent if another
// row's drag payload was dropped onto it -- the same "one user gesture per
// frame" assumption InspectorResult's EntityAction already makes. Actually
// executing any of these (CreateEntity, AttachChild/DetachChild,
// DestroyEntityGraph) is deferred to the caller in main.cpp, which owns the
// SceneManager this Registry belongs to; mutating the Hierarchy mid-walk
// here would invalidate the child list this recursion is still iterating.
void DrawEntityTreeNode(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity,
    asge::ecs::Entity& ioSelected, EntityListResult& ioResult, GroupContext& ioGroups ) noexcept
{
    auto const hierarchy = inRegistry.GetComponent<asge::ecs::components::Hierarchy>( inEntity );
    bool const isRoot = !hierarchy || hierarchy.Value().get().m_Parent == asge::ecs::Entity::Null();
    bool const hasChildren = hierarchy && hierarchy.Value().get().m_FirstChild != asge::ecs::Entity::Null();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if ( inEntity == ioSelected ) flags |= ImGuiTreeNodeFlags_Selected;
    if ( !hasChildren ) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // "##<index>" suffix keeps each row's ImGui ID unique even when two
    // entities share the same (or default, unnamed) display label -- the
    // same class of collision DrawSection's PushID guards against.
    std::string const label = GetEntityLabel( inRegistry, inEntity ) + "##" + std::to_string( inEntity.m_Index );
    bool const open = ImGui::TreeNodeEx( label.c_str(), flags );

    // Phase 15: shown for this row's own Disable marker, or any descendant's
    // -- so a disabled entity N levels deep still surfaces up through every
    // collapsed ancestor above it, not just its own row.
    if ( SubtreeHasDisabled( inRegistry, inEntity ) )
    {
        ImVec2 const rowMin = ImGui::GetItemRectMin();
        ImVec2 const rowMax = ImGui::GetItemRectMax();
        ImVec2 const eyeCenter{ ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 18.0f, ( rowMin.y + rowMax.y ) * 0.5f };
        DrawClosedEyeIcon( ImGui::GetWindowDrawList(), eyeCenter, 6.0f, ImGui::GetColorU32( ImGuiCol_TextDisabled ) );
    }

    // OpenOnArrow means a click on the label itself (not the arrow) reaches
    // here without also toggling open/closed -- exactly "select this row".
    if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) ioSelected = inEntity;

    if ( ImGui::BeginDragDropSource() )
    {
        ImGui::SetDragDropPayload( "ASGE_ENTITY", &inEntity, sizeof( inEntity ) );
        ImGui::TextUnformatted( label.c_str() );
        ImGui::EndDragDropSource();
    }
    if ( ImGui::BeginDragDropTarget() )
    {
        // Cycle/no-op guards (dropping onto itself, onto one of its own
        // descendants, or to where it already is) live in AttachChild
        // itself -- not duplicated here, the drop always just reports the
        // gesture and lets the caller's AttachChild call decide.
        if ( auto const* payload = ImGui::AcceptDragDropPayload( "ASGE_ENTITY" ) )
        {
            asge::ecs::Entity dragged;
            std::memcpy( &dragged, payload->Data, sizeof( dragged ) );
            ioResult.m_Action = HierarchyAction::Reparent;
            ioResult.m_Target = dragged;
            ioResult.m_NewParent = inEntity;
        }
        ImGui::EndDragDropTarget();
    }

    if ( ImGui::BeginPopupContextItem() )
    {
        {
            // An entity can only join a group of its own scope: a top-level entity a
            // top-level group, a child a group under that same parent.
            auto const parent = hierarchy ? hierarchy.Value().get().m_Parent : asge::ecs::Entity::Null();
            if ( ImGui::BeginMenu( "Move to Group" ) )
            {
                bool listed = false;
                for ( std::size_t i = 0; i < ioGroups.m_Groups.size(); ++i )
                {
                    if ( ioGroups.m_Groups[i].m_Parent != parent ) continue;
                    listed = true;
                    std::string const item = ioGroups.m_Groups[i].m_Name + "##move" + std::to_string( i );
                    if ( ImGui::MenuItem( item.c_str() ) ) ioGroups.m_Op = { GroupOpKind::MoveTo, i, inEntity };
                }
                if ( listed ) ImGui::Separator();
                if ( ImGui::MenuItem( "New Group..." ) )
                {
                    ioGroups.m_Op = { GroupOpKind::NewWith, 0, inEntity };
                    ioGroups.m_PromptForName = true;
                }
                ImGui::EndMenu();
            }
            bool const grouped = FindGroupOf( ioGroups.m_Groups, inEntity ) != kNoGroup;
            if ( ImGui::MenuItem( "Remove from Group", nullptr, false, grouped ) )
                ioGroups.m_Op = { GroupOpKind::RemoveFrom, 0, inEntity };
            if ( hasChildren && ImGui::MenuItem( "New Child Group..." ) )
            {
                ioGroups.m_Op = { GroupOpKind::NewEmpty, 0, inEntity }; // scoped to this entity's children
                ioGroups.m_PromptForName = true;
            }
            ImGui::Separator();
        }
        if ( ImGui::MenuItem( "New Child" ) )
        {
            ioResult.m_Action = HierarchyAction::NewChild;
            ioResult.m_Target = inEntity;
        }
        if ( ImGui::MenuItem( "Detach", nullptr, false, !isRoot ) )
        {
            ioResult.m_Action = HierarchyAction::Detach;
            ioResult.m_Target = inEntity;
        }
        if ( ImGui::MenuItem( "Remove" ) )
        {
            ioResult.m_Action = HierarchyAction::Remove;
            ioResult.m_Target = inEntity;
        }
        ImGui::EndPopup();
    }

    if ( open && hasChildren )
    {
        // Snapshotted up front, not walked live -- ioResult's action (if any
        // gets set this frame) is only applied by the caller after this
        // whole tree finishes drawing, so the Hierarchy itself never
        // changes mid-recursion; this is just ForEachChild's own contract
        // (safe to reparent/detach the entity currently being visited, not
        // safe to assume its sibling links survive an arbitrary mutation).
        std::vector<asge::ecs::Entity> children;
        asge::ecs::components::ForEachChild(
            inRegistry, inEntity, [&]( asge::ecs::Entity inChild ) { children.push_back( inChild ); } );
        DrawScope( inRegistry, inEntity, children, ioSelected, ioResult, ioGroups );
    }
    if ( open && hasChildren ) ImGui::TreePop();
}

// One group folder of DrawEntityListPanel: its members as ordinary tree rows
// underneath, a drop target for rows dragged onto it, and its own menu.
void DrawGroupNode(
    asge::ecs::Registry& inRegistry, EntityGroup& ioGroup, std::size_t inIndex,
    asge::ecs::Entity& ioSelected, EntityListResult& ioResult, GroupContext& ioGroups ) noexcept
{
    // The folder's open state lives in the group; forcing it each frame and
    // reading back what the click made of it keeps the two in step.
    ImGui::SetNextItemOpen( ioGroup.m_Open, ImGuiCond_Always );
    std::string const label =
        ioGroup.m_Name + " (" + std::to_string( ioGroup.m_Members.size() ) + ")##group" + std::to_string( inIndex );
    ioGroup.m_Open = ImGui::TreeNodeEx( label.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth );

    if ( ImGui::BeginDragDropTarget() )
    {
        if ( auto const* payload = ImGui::AcceptDragDropPayload( "ASGE_ENTITY" ) )
        {
            asge::ecs::Entity dragged;
            std::memcpy( &dragged, payload->Data, sizeof( dragged ) );
            ioGroups.m_Op = { GroupOpKind::MoveTo, inIndex, dragged };
        }
        ImGui::EndDragDropTarget();
    }

    if ( ImGui::BeginPopupContextItem() )
    {
        if ( ImGui::MenuItem( "Rename..." ) )
        {
            ioGroups.m_Op = { GroupOpKind::Rename, inIndex, asge::ecs::Entity::Null() };
            ioGroups.m_PromptForName = true;
        }
        if ( ImGui::MenuItem( "Delete Group" ) ) ioGroups.m_Op = { GroupOpKind::Delete, inIndex, asge::ecs::Entity::Null() };
        ImGui::EndPopup();
    }

    if ( !ioGroup.m_Open ) return;

    auto const members = ioGroup.m_Members; // copied: drawing a row may queue, never apply, a change
    for ( auto member : members ) DrawEntityTreeNode( inRegistry, member, ioSelected, ioResult, ioGroups );
    ImGui::TreePop();
}

void DrawScope(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inScope, std::vector<asge::ecs::Entity> const& inChildren,
    asge::ecs::Entity& ioSelected, EntityListResult& ioResult, GroupContext& ioGroups ) noexcept
{
    for ( std::size_t i = 0; i < ioGroups.m_Groups.size(); ++i )
    {
        if ( ioGroups.m_Groups[i].m_Parent == inScope )
            DrawGroupNode( inRegistry, ioGroups.m_Groups[i], i, ioSelected, ioResult, ioGroups );
    }
    for ( auto child : inChildren )
    {
        if ( FindGroupOf( ioGroups.m_Groups, child ) == kNoGroup )
            DrawEntityTreeNode( inRegistry, child, ioSelected, ioResult, ioGroups );
    }
}

}

// Declared in Inspector.hpp (Phase 16) so main.cpp's Create UI Element modal
// can reuse the exact same widget for its Font Path field, not a second,
// hand-typed one -- see the header doc comment for the shape/behavior.
bool DrawAssetPathCombo( char const* inLabel, std::string& ioPath, std::vector<std::string> const& inKnownPaths ) noexcept
{
    std::vector<char const*> items;
    items.reserve( inKnownPaths.size() + 1 );
    items.push_back( "None" );
    for ( auto const& path : inKnownPaths ) items.push_back( path.c_str() );

    int current = 0;
    for ( std::size_t i = 0; i < inKnownPaths.size(); ++i )
    {
        if ( inKnownPaths[i] == ioPath ) { current = static_cast<int>( i ) + 1; break; }
    }

    if ( !ImGui::Combo( inLabel, &current, items.data(), static_cast<int>( items.size() ) ) ) return false;

    ioPath = current == 0 ? std::string{} : inKnownPaths[current - 1];
    return true;
}

// inEntity's Name::m_Name if it has one and it's non-empty, else "Entity #N"
// ("Button #N"/"Label #N" for a UIButton/UILabel entity, Phase 16 -- checked
// in that order since a Button's own caption text also carries a UILabel) --
// used for both the entity list and the inspector header, so the two panels
// never disagree about what to call an entity. Declared in Inspector.hpp
// (Phase 14) so AssetBrowser.cpp's "Attach To" submenu can label entities
// the same way.
std::string GetEntityLabel( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
{
    auto const displayId = GetOrAssignDisplayId( inEntity ); // always assigned, even if Name ends up used instead
    if ( auto r = inRegistry.GetComponent<Name>( inEntity ); r && !r.Value().get().m_Name.empty() )
    {
        return r.Value().get().m_Name;
    }

    char const* prefix = "Entity";
    if ( inRegistry.HasComponent<UIButton>( inEntity ) ) prefix = "Button";
    else if ( inRegistry.HasComponent<UICheckbox>( inEntity ) ) prefix = "Checkbox";
    else if ( inRegistry.HasComponent<UISlider>( inEntity ) ) prefix = "Slider";
    else if ( inRegistry.HasComponent<UIPanel>( inEntity ) ) prefix = "Panel";
    else if ( inRegistry.HasComponent<UILabel>( inEntity ) ) prefix = "Label";

    char buf[32];
    std::snprintf( buf, sizeof(buf), "%s #%u", prefix, displayId );
    return buf;
}

void ResetEntityDisplayIds() noexcept
{
    g_EntityDisplayIds.clear();
    g_NextEntityDisplayId = 0;
}

EntityListResult DrawEntityListPanel(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity& ioSelected, bool inHasProject,
    EntityGroupList& ioGroups ) noexcept
{
    // Which group operation the open Group Name modal will apply once it gets a name.
    static GroupOp s_NameOp;
    static char    s_NameBuffer[64] = "";

    // Anchored flush to the right edge, re-snapping only on an actual
    // resize -- see AnchorCondOnResize's own doc comment.
    float const rightX = ImGui::GetIO().DisplaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
    ImGui::SetNextWindowPos( ImVec2( rightX, 95.0f ), AnchorCondOnResize() );
    ImGui::SetNextWindowSize( ImVec2( kEditorPanelWidth, 160.0f ), ImGuiCond_FirstUseEver );

    EntityListResult result;

    ImGui::Begin( "Entities" );
    if ( !inHasProject ) ImGui::BeginDisabled();
    result.m_CreateClicked = ImGui::Button( "Create Entity" );
    ImGui::SameLine();
    // Phase 16: the actual Button/Label construction goes through main.cpp's
    // Type Selection -> Creation modal flow (UI::CreateButton/CreateLabel),
    // not here -- this panel only reports the click, same division of labor
    // as "Create Entity" itself.
    result.m_CreateUIElementClicked = ImGui::Button( "Create UI Element" );
    bool const createGroupClicked = ImGui::Button( "Create Group" );
    if ( !inHasProject ) ImGui::EndDisabled();
    ImGui::Separator();

    // Root entities only (no Hierarchy, or one with no parent) -- every
    // other entity is reached recursively, as some root's descendant,
    // through DrawEntityTreeNode's own ForEachChild walk. AllEntities() is a
    // raw storage-slot scan (ascending Entity::m_Index), not creation order
    // -- listing roots in that order let a newly created one land in a low,
    // just-freed slot and appear above older ones instead of after them.
    // Sorted by the same creation-order id GetEntityLabel's numbering is
    // built on instead.
    auto entities = inRegistry.AllEntities();
    std::vector<asge::ecs::Entity> roots;
    for ( auto entity : entities )
    {
        auto const hierarchy = inRegistry.GetComponent<asge::ecs::components::Hierarchy>( entity );
        if ( !hierarchy || hierarchy.Value().get().m_Parent == asge::ecs::Entity::Null() ) roots.push_back( entity );
    }
    std::sort( roots.begin(), roots.end(), []( asge::ecs::Entity inA, asge::ecs::Entity inB ) noexcept
    {
        return GetOrAssignDisplayId( inA ) < GetOrAssignDisplayId( inB );
    } );

    // A group whose parent is gone, or a member that was deleted or moved under another
    // parent, no longer belongs -- drop it rather than show a stale row.
    std::unordered_set<asge::ecs::Entity> const alive( entities.begin(), entities.end() );
    std::erase_if( ioGroups, [&]( EntityGroup const& inGroup )
    {
        return inGroup.m_Parent != asge::ecs::Entity::Null() && !alive.contains( inGroup.m_Parent );
    } );
    for ( auto& group : ioGroups )
    {
        std::erase_if( group.m_Members, [&]( asge::ecs::Entity inE )
        {
            return !alive.contains( inE ) || ParentOf( inRegistry, inE ) != group.m_Parent;
        } );
    }

    GroupContext groupCtx{ ioGroups, {}, false };
    if ( createGroupClicked )
    {
        groupCtx.m_Op = { GroupOpKind::NewEmpty, 0, asge::ecs::Entity::Null() };
        groupCtx.m_PromptForName = true;
    }

    DrawScope( inRegistry, asge::ecs::Entity::Null(), roots, ioSelected, result, groupCtx );

    // Applied only now that nothing is iterating the groups any more.
    auto const& op = groupCtx.m_Op;
    switch ( op.m_Kind )
    {
    case GroupOpKind::MoveTo:
        if ( op.m_Group < ioGroups.size() && alive.contains( op.m_Entity )
          && ParentOf( inRegistry, op.m_Entity ) == ioGroups[op.m_Group].m_Parent )
        {
            RemoveFromAllGroups( ioGroups, op.m_Entity );
            ioGroups[op.m_Group].m_Members.push_back( op.m_Entity );
            result.m_GroupsChanged = true;
        }
        break;
    case GroupOpKind::RemoveFrom:
        RemoveFromAllGroups( ioGroups, op.m_Entity );
        result.m_GroupsChanged = true;
        break;
    case GroupOpKind::Delete:
        if ( op.m_Group < ioGroups.size() )
        {
            ioGroups.erase( ioGroups.begin() + static_cast<std::ptrdiff_t>( op.m_Group ) );
            result.m_GroupsChanged = true;
        }
        break;
    default:
        break;
    }

    // The name prompt is opened here, at the window's own ID level -- opening it from
    // inside a row's popup would hash to a different ID than BeginPopupModal below.
    if ( groupCtx.m_PromptForName )
    {
        s_NameOp = groupCtx.m_Op;
        if ( s_NameOp.m_Kind == GroupOpKind::Rename && s_NameOp.m_Group < ioGroups.size() )
            std::snprintf( s_NameBuffer, sizeof( s_NameBuffer ), "%s", ioGroups[s_NameOp.m_Group].m_Name.c_str() );
        else
            std::snprintf( s_NameBuffer, sizeof( s_NameBuffer ), "%s", "New Group" );
        ImGui::OpenPopup( "Group Name" );
    }

    if ( ImGui::BeginPopupModal( "Group Name", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        if ( ImGui::IsWindowAppearing() ) ImGui::SetKeyboardFocusHere();
        bool const submitted = ImGui::InputText( "##GroupName", s_NameBuffer, sizeof( s_NameBuffer ), ImGuiInputTextFlags_EnterReturnsTrue );

        bool const canApply = s_NameBuffer[0] != '\0';
        if ( !canApply ) ImGui::BeginDisabled();
        bool const okClicked = ImGui::Button( "OK" );
        if ( !canApply ) ImGui::EndDisabled();
        ImGui::SameLine();
        bool const cancelled = ImGui::Button( "Cancel" );

        if ( canApply && ( submitted || okClicked ) )
        {
            switch ( s_NameOp.m_Kind )
            {
            case GroupOpKind::NewEmpty: // m_Entity is the parent whose children it holds, Null() for the top level
                if ( s_NameOp.m_Entity == asge::ecs::Entity::Null() || alive.contains( s_NameOp.m_Entity ) )
                    ioGroups.push_back( EntityGroup{ s_NameBuffer, {}, true, s_NameOp.m_Entity } );
                break;
            case GroupOpKind::NewWith:
                if ( alive.contains( s_NameOp.m_Entity ) )
                {
                    auto const parent = ParentOf( inRegistry, s_NameOp.m_Entity );
                    RemoveFromAllGroups( ioGroups, s_NameOp.m_Entity );
                    ioGroups.push_back( EntityGroup{ s_NameBuffer, { s_NameOp.m_Entity }, true, parent } );
                }
                break;
            case GroupOpKind::Rename:
                if ( s_NameOp.m_Group < ioGroups.size() ) ioGroups[s_NameOp.m_Group].m_Name = s_NameBuffer;
                break;
            default:
                break;
            }
            result.m_GroupsChanged = true;
            ImGui::CloseCurrentPopup();
        }
        else if ( cancelled )
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();

    return result;
}

InspectorResult DrawInspectorPanel(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected,
    std::vector<std::string> const& inKnownTextures,
    std::vector<std::string> const& inKnownAnimations,
    std::vector<std::string> const& inKnownAudio,
    std::vector<std::string> const& inKnownFonts,
    WaypointEditState& ioWaypointEdit,
    ColliderDrawState& ioColliderDraw ) noexcept
{
    if ( inSelected == asge::ecs::Entity::Null() ) return {};

    // Anchored flush to the right edge, re-snapping only on an actual
    // resize -- see AnchorCondOnResize's own doc comment.
    float const rightX = ImGui::GetIO().DisplaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
    float const remainingHeight = ImGui::GetIO().DisplaySize.y - 265.0f - 20.0f; // fills down to a bottom margin
    ImGui::SetNextWindowPos( ImVec2( rightX, 265.0f ), AnchorCondOnResize() );
    ImGui::SetNextWindowSize( ImVec2( kEditorPanelWidth, remainingHeight ), ImGuiCond_FirstUseEver );

    EntityAction action = EntityAction::None;

    ImGui::Begin( "Inspector" );

    // Two independent accumulators (bitwise-OR, not ||, so every section
    // still draws even once one reports a change -- short-circuiting would
    // skip the rest of the entity's components for the remainder of this
    // frame). componentsChanged keeps its original, narrow meaning (Add/
    // Remove plus an asset-path selection change) and still drives
    // EntityAction::ComponentsChanged/ResolveAssets exactly as before;
    // fieldChanged (Phase 11) is fed by every section regardless of type,
    // for InspectorResult::m_FieldChanged -- see its own doc comment for
    // why the two can't just be the same signal.
    bool componentsChanged = false;
    bool fieldChanged = false;

    // Phase 15: this entity's own Disable marker only, not the inherited
    // Registry::IsDisabled() effective state -- a child of a disabled
    // ancestor has nothing of its own to toggle here (the (inherited) hint
    // below covers that case instead). Recursively disabling children isn't
    // this checkbox's job either: IsDisabled() already walks up through
    // Hierarchy::m_Parent on its own, so a subtree root's Disable alone is
    // enough for every descendant to already read as disabled.
    bool ownDisable = inRegistry.HasComponent<asge::ecs::markers::Disable>( inSelected );
    if ( ImGui::Checkbox( "Disabled", &ownDisable ) )
    {
        if ( ownDisable )
        {
            inRegistry.DisableEntity( inSelected );
        }
        else if ( auto const result = inRegistry.RemoveComponent<asge::ecs::markers::Disable>( inSelected ); !result )
        {
            result.LogError();
        }
        fieldChanged = true;
    }
    else if ( !ownDisable && inRegistry.IsDisabled( inSelected ) )
    {
        ImGui::SameLine();
        ImGui::TextDisabled( "(inherited from parent)" );
    }

    ImGui::Text( "%s", GetEntityLabel( inRegistry, inSelected ).c_str() );

    // Phase 13: read-only -- reparenting happens through the Entities tree's
    // drag-and-drop/context menu, not here.
    if ( auto const hierarchy = inRegistry.GetComponent<asge::ecs::components::Hierarchy>( inSelected );
         hierarchy && hierarchy.Value().get().m_Parent != asge::ecs::Entity::Null() )
    {
        ImGui::TextDisabled( "Parent: %s", GetEntityLabel( inRegistry, hierarchy.Value().get().m_Parent ).c_str() );
    }

    if ( ImGui::Button( "Duplicate" ) ) action = EntityAction::Duplicate;
    ImGui::SameLine();
    if ( ImGui::Button( "Delete" ) ) action = EntityAction::Delete;

    ImGui::Separator();

    bool const nameChanged = DrawSection<Name>( inRegistry, inSelected, "Name" );
    fieldChanged |= nameChanged;
    bool const transformChanged = DrawSection<Transform>( inRegistry, inSelected, "Transform" );
    fieldChanged |= transformChanged;
    bool const velocityChanged = DrawSection<Velocity>( inRegistry, inSelected, "Velocity" );
    fieldChanged |= velocityChanged;
    bool const rigidbodyChanged = DrawSection<Rigidbody>( inRegistry, inSelected, "Rigidbody" );
    fieldChanged |= rigidbodyChanged;
    bool const spriteChanged = DrawSection<Sprite>( inRegistry, inSelected, "Sprite", inKnownTextures );
    componentsChanged |= spriteChanged; fieldChanged |= spriteChanged;
    bool const renderInfoChanged = DrawSection<RenderInfo>(
        inRegistry, inSelected, "RenderInfo", ParentOf( inRegistry, inSelected ) != asge::ecs::Entity::Null() );
    fieldChanged |= renderInfoChanged;
    bool const colliderChanged = DrawSection<Collider>( inRegistry, inSelected, "Collider", inSelected, ioColliderDraw );
    fieldChanged |= colliderChanged;
    bool const cameraChanged = DrawSection<Camera>( inRegistry, inSelected, "Camera" );
    fieldChanged |= cameraChanged;
    bool const audioChanged = DrawSection<AudioSource>( inRegistry, inSelected, "AudioSource", inKnownAudio );
    componentsChanged |= audioChanged; fieldChanged |= audioChanged;
    bool const animationChanged = DrawSection<Animation>( inRegistry, inSelected, "Animation", inKnownAnimations );
    componentsChanged |= animationChanged; fieldChanged |= animationChanged;
    bool const pathFollowChanged = DrawSection<PathFollow>( inRegistry, inSelected, "PathFollow", inSelected, ioWaypointEdit );
    fieldChanged |= pathFollowChanged;

    // Phase 16: UIRect/Interactable/UIButton/UILabel -- view/edit-only, see
    // DrawSection's own "no entry -> no remove button" fallback above for why
    // these never get an "x" the way every section before this does.
    bool const uiRectChanged = DrawSection<UIRect>( inRegistry, inSelected, "UIRect", inRegistry, inSelected );
    fieldChanged |= uiRectChanged;
    bool const interactableChanged = DrawSection<Interactable>( inRegistry, inSelected, "Interactable" );
    fieldChanged |= interactableChanged;
    bool const uiButtonChanged = DrawSection<UIButton>( inRegistry, inSelected, "UIButton" );
    fieldChanged |= uiButtonChanged;
    bool const uiLabelChanged = DrawSection<UILabel>( inRegistry, inSelected, "UILabel", inKnownFonts );
    componentsChanged |= uiLabelChanged; fieldChanged |= uiLabelChanged;
    // Phase 17: UICheckbox/UISlider/UIPanel are view/edit-only like the above;
    // UILayoutItem is the exception -- optional per child, so it does get the
    // generic Add/Remove entry.
    bool const uiCheckboxChanged = DrawSection<UICheckbox>( inRegistry, inSelected, "UICheckbox" );
    fieldChanged |= uiCheckboxChanged;
    bool const uiSliderChanged = DrawSection<UISlider>( inRegistry, inSelected, "UISlider" );
    fieldChanged |= uiSliderChanged;
    bool const uiPanelChanged = DrawSection<UIPanel>( inRegistry, inSelected, "UIPanel" );
    fieldChanged |= uiPanelChanged;
    bool const uiLayoutItemChanged = DrawSection<UILayoutItem>( inRegistry, inSelected, "UILayoutItem" );
    fieldChanged |= uiLayoutItemChanged;

    bool const addComponentClicked = DrawAddComponentControl( inRegistry, inSelected, inKnownTextures );
    componentsChanged |= addComponentClicked; fieldChanged |= addComponentClicked;

    ImGui::End();

    if ( action == EntityAction::None && componentsChanged ) action = EntityAction::ComponentsChanged;
    return { action, fieldChanged || action != EntityAction::None };
}
