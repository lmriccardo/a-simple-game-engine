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

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>

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

// inEntity's Name::m_Name if it has one and it's non-empty, else "Entity #N"
// -- used for both the entity list and the inspector header, so the two
// panels never disagree about what to call an entity.
std::string GetEntityLabel( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
{
    auto const displayId = GetOrAssignDisplayId( inEntity ); // always assigned, even if Name ends up used instead
    if ( auto r = inRegistry.GetComponent<Name>( inEntity ); r && !r.Value().get().m_Name.empty() )
    {
        return r.Value().get().m_Name;
    }

    char buf[32];
    std::snprintf( buf, sizeof(buf), "Entity #%u", displayId );
    return buf;
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

// A dropdown restricted to inKnownPaths plus a leading "None" entry -- same
// shape as DrawAddComponentControl's own "##SpriteTexture" combo below, but
// with "None" for "no asset assigned yet" (Add-Component's combo has no such
// option since a Sprite there always gets a real texture up front). The
// current selection is whichever inKnownPaths entry equals ioPath (or
// "None" if it's empty or not present in the list, e.g. a freshly-added
// component). ImGui::Combo only returns true on the frame the picked index
// actually changes, so this naturally resolves on selection-changed rather
// than on every frame or keystroke.
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

void DrawInspector( Name& inName ) noexcept
{
    DrawTextField( "Name", inName.m_Name );
}

void DrawInspector( Transform& inT ) noexcept
{
    float pos[2]{ inT.m_X, inT.m_Y };
    if ( ImGui::DragFloat2( "Position", pos ) ) { inT.m_X = pos[0]; inT.m_Y = pos[1]; }

    ImGui::DragFloat( "Rotation (rad)", &inT.m_Rotation, 0.01f );

    float scale[2]{ inT.m_ScaleX, inT.m_ScaleY };
    if ( ImGui::DragFloat2( "Scale", scale ) ) { inT.m_ScaleX = scale[0]; inT.m_ScaleY = scale[1]; }
}

void DrawInspector( Velocity& inVelocity ) noexcept
{
    float vel[2]{ inVelocity.m_DX, inVelocity.m_DY };
    if ( ImGui::DragFloat2( "Velocity", vel ) ) { inVelocity.m_DX = vel[0]; inVelocity.m_DY = vel[1]; }
}

void DrawInspector( Rigidbody& inRigidbody ) noexcept
{
    ImGui::DragFloat( "Mass", &inRigidbody.m_Mass, 0.1f, 0.0f, 1000.0f );
    ImGui::Checkbox( "Affected By Gravity", &inRigidbody.m_AffectedByGravity );
}

// m_SourceRect editing is out of scope here (asset-browsing/viewport gizmo
// territory). m_VirtualPath is a dropdown restricted to inKnownTextures
// (Phase 10) -- its return reports only whether the path selection changed,
// not m_Layer/m_YSort edits, since only a path change needs AssetManager::
// ResolveAssets re-run.
bool DrawInspector( Sprite& inSprite, std::vector<std::string> const& inKnownTextures ) noexcept
{
    bool const pathChanged = DrawAssetPathCombo( "Virtual Path", inSprite.m_VirtualPath, inKnownTextures );

    int layer = inSprite.m_Layer;
    if ( ImGui::DragInt( "Layer", &layer ) ) inSprite.m_Layer = layer;

    ImGui::Checkbox( "Y-Sort", &inSprite.m_YSort );
    return pathChanged;
}

void DrawInspector( Collider& inCollider ) noexcept
{
    if ( auto* rect = std::get_if<asge::math::Rect>( &inCollider.m_LocalBounds ) )
    {
        float vals[4]{ rect->m_X, rect->m_Y, rect->m_Width, rect->m_Height };
        if ( ImGui::DragFloat4( "Rect (x,y,w,h)", vals ) )
        {
            rect->m_X = vals[0]; rect->m_Y = vals[1]; rect->m_Width = vals[2]; rect->m_Height = vals[3];
        }
    }
    else if ( auto* circle = std::get_if<asge::math::Circle>( &inCollider.m_LocalBounds ) )
    {
        float center[2]{ circle->m_Center.x(), circle->m_Center.y() };
        if ( ImGui::DragFloat2( "Center", center ) )
        {
            circle->m_Center = asge::math::Float2{ center[0], center[1] };
        }
        ImGui::DragFloat( "Radius", &circle->m_Radius );
    }

    static char const* const kResolutionNames[]{ "Unknown", "Trigger", "Solid" };
    int resIndex = static_cast<int>( inCollider.m_Resolution );
    if ( ImGui::Combo( "Resolution", &resIndex, kResolutionNames, 3 ) )
    {
        inCollider.m_Resolution = static_cast<ResolutionType>( resIndex );
    }

    ImGui::InputScalar( "Layer", ImGuiDataType_U32, &inCollider.m_Layer );
    ImGui::InputScalar( "Mask", ImGuiDataType_U32, &inCollider.m_Mask );
}

void DrawInspector( Camera& inCamera ) noexcept
{
    ImGui::DragFloat( "Zoom", &inCamera.m_Zoom, 0.01f, 0.01f, 100.0f );
    ImGui::DragFloat( "Smoothing", &inCamera.m_Smoothing, 0.01f, 0.0f, 100.0f );
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
// only a clip change needs AssetManager::ResolveAssets re-run.
bool DrawInspector( Animation& inAnimation, std::vector<std::string> const& inKnownAnimations ) noexcept
{
    bool const clipChanged = DrawAssetPathCombo( "Clip Path", inAnimation.m_ClipPath, inKnownAnimations );
    ImGui::DragFloat( "Frame Duration", &inAnimation.m_FrameDuration, 0.01f, 0.0f, 10.0f );
    return clipChanged;
}

// Waypoint list editing is skipped -- a raw DragFloat2 list is a poor way to
// author a path; that's viewport gizmo territory (Phase 4), not a plain
// inspector field. Only the scalar fields that round-trip alongside it
// (m_Speed/m_Loop/m_Resolution) are edited here.
void DrawInspector( PathFollow& inPathFollow ) noexcept
{
    ImGui::Text( "Waypoints: %zu (editing not yet supported)", inPathFollow.m_Waypoints.size() );
    ImGui::DragFloat( "Speed", &inPathFollow.m_Speed );
    ImGui::Checkbox( "Loop", &inPathFollow.m_Loop );

    int resolution = static_cast<int>( inPathFollow.m_Resolution );
    if ( ImGui::DragInt( "Resolution", &resolution, 1.0f, 1, 256 ) )
    {
        inPathFollow.m_Resolution = static_cast<std::size_t>( resolution );
    }
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
        { inRegistry.AddComponent<T>( inEntity, T{} ); },
        []( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
        { if ( auto const result = inRegistry.RemoveComponent<T>( inEntity ); !result ) result.LogError(); }
    };
}

ComponentEntry const kComponentEntries[]{
    MakeComponentEntry<Name>( "Name" ),
    MakeComponentEntry<Transform>( "Transform" ),
    MakeComponentEntry<Velocity>( "Velocity" ),
    MakeComponentEntry<Rigidbody>( "Rigidbody" ),
    MakeComponentEntry<Sprite>( "Sprite" ),
    MakeComponentEntry<Collider>( "Collider" ),
    MakeComponentEntry<Camera>( "Camera" ),
    MakeComponentEntry<AudioSource>( "AudioSource" ),
    MakeComponentEntry<Animation>( "Animation" ),
    MakeComponentEntry<PathFollow>( "PathFollow" ),
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
        ImGui::SameLine( ImGui::GetWindowWidth() - 30.0f );
        if ( ImGui::SmallButton( "x" ) )
        {
            FindComponentEntry( inName )->m_Remove( inRegistry, inEntity );
            ImGui::PopID();
            return true;
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

}

void ResetEntityDisplayIds() noexcept
{
    g_EntityDisplayIds.clear();
    g_NextEntityDisplayId = 0;
}

bool DrawEntityListPanel( asge::ecs::Registry& inRegistry, asge::ecs::Entity& ioSelected ) noexcept
{
    // Anchored flush to the right edge, same reasoning as main.cpp's Scene
    // panel above it -- see its comment.
    float const rightX = ImGui::GetIO().DisplaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
    ImGui::SetNextWindowPos( ImVec2( rightX, 95.0f ), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2( kEditorPanelWidth, 160.0f ), ImGuiCond_FirstUseEver );

    ImGui::Begin( "Entities" );
    bool const createClicked = ImGui::Button( "Create Entity" );
    ImGui::Separator();

    // AllEntities() is a raw storage-slot scan (ascending Entity::m_Index),
    // not creation order -- listing in that order let a newly created
    // entity land in a low, just-freed slot and appear above older ones
    // instead of after them. Sorted by the same creation-order id
    // GetEntityLabel's numbering is built on instead.
    auto entities = inRegistry.AllEntities();
    std::sort( entities.begin(), entities.end(), []( asge::ecs::Entity inA, asge::ecs::Entity inB ) noexcept
    {
        return GetOrAssignDisplayId( inA ) < GetOrAssignDisplayId( inB );
    } );

    for ( auto entity : entities )
    {
        // "##<index>" suffix keeps each row's ImGui ID unique even when two
        // entities share the same (or default, unnamed) display label --
        // the same class of collision DrawSection's PushID guards against.
        std::string const label = GetEntityLabel( inRegistry, entity )
            + "##" + std::to_string( entity.m_Index );
        if ( ImGui::Selectable( label.c_str(), entity == ioSelected ) ) ioSelected = entity;
    }
    ImGui::End();

    return createClicked;
}

EntityAction DrawInspectorPanel(
    asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected,
    std::vector<std::string> const& inKnownTextures,
    std::vector<std::string> const& inKnownAnimations,
    std::vector<std::string> const& inKnownAudio ) noexcept
{
    if ( inSelected == asge::ecs::Entity::Null() ) return EntityAction::None;

    // Anchored flush to the right edge, same reasoning as main.cpp's Scene
    // panel above it -- see its comment.
    float const rightX = ImGui::GetIO().DisplaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
    float const remainingHeight = ImGui::GetIO().DisplaySize.y - 265.0f - 20.0f; // fills down to a bottom margin
    ImGui::SetNextWindowPos( ImVec2( rightX, 265.0f ), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2( kEditorPanelWidth, remainingHeight ), ImGuiCond_FirstUseEver );

    EntityAction action = EntityAction::None;

    ImGui::Begin( "Inspector" );
    ImGui::Text( "%s", GetEntityLabel( inRegistry, inSelected ).c_str() );

    if ( ImGui::Button( "Duplicate" ) ) action = EntityAction::Duplicate;
    ImGui::SameLine();
    if ( ImGui::Button( "Delete" ) ) action = EntityAction::Delete;

    ImGui::Separator();

    // Bitwise-OR (not ||, so every section still draws even once one
    // reports a change -- short-circuiting would skip the rest of the
    // entity's components for the remainder of this frame).
    bool componentsChanged = false;
    componentsChanged |= DrawSection<Name>( inRegistry, inSelected, "Name" );
    componentsChanged |= DrawSection<Transform>( inRegistry, inSelected, "Transform" );
    componentsChanged |= DrawSection<Velocity>( inRegistry, inSelected, "Velocity" );
    componentsChanged |= DrawSection<Rigidbody>( inRegistry, inSelected, "Rigidbody" );
    componentsChanged |= DrawSection<Sprite>( inRegistry, inSelected, "Sprite", inKnownTextures );
    componentsChanged |= DrawSection<Collider>( inRegistry, inSelected, "Collider" );
    componentsChanged |= DrawSection<Camera>( inRegistry, inSelected, "Camera" );
    componentsChanged |= DrawSection<AudioSource>( inRegistry, inSelected, "AudioSource", inKnownAudio );
    componentsChanged |= DrawSection<Animation>( inRegistry, inSelected, "Animation", inKnownAnimations );
    componentsChanged |= DrawSection<PathFollow>( inRegistry, inSelected, "PathFollow" );

    componentsChanged |= DrawAddComponentControl( inRegistry, inSelected, inKnownTextures );

    ImGui::End();

    if ( action == EntityAction::None && componentsChanged ) action = EntityAction::ComponentsChanged;
    return action;
}
