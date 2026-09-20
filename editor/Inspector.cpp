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

#include <cstdio>
#include <string>

using namespace asge::game::components;

namespace
{

// inEntity's Name::m_Name if it has one and it's non-empty, else "Entity #N"
// -- used for both the entity list and the inspector header, so the two
// panels never disagree about what to call an entity.
std::string GetEntityLabel( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
{
    if ( auto r = inRegistry.GetComponent<Name>( inEntity ); r && !r.Value().get().m_Name.empty() )
    {
        return r.Value().get().m_Name;
    }

    char buf[32];
    std::snprintf( buf, sizeof(buf), "Entity #%u", inEntity.m_Index );
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

// Only m_VirtualPath/m_Layer/m_YSort are edited here -- m_SourceRect editing
// and re-resolving a changed path into a live m_Texture are asset-browsing
// concerns for Phase 6, not this generalized-inspector phase.
void DrawInspector( Sprite& inSprite ) noexcept
{
    DrawTextField( "Virtual Path", inSprite.m_VirtualPath );

    int layer = inSprite.m_Layer;
    if ( ImGui::DragInt( "Layer", &layer ) ) inSprite.m_Layer = layer;

    ImGui::Checkbox( "Y-Sort", &inSprite.m_YSort );
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
void DrawInspector( AudioSource& inAudioSource ) noexcept
{
    DrawTextField( "Clip Path", inAudioSource.m_VirtualClipPath );
}

// Only m_ClipPath/m_FrameDuration round-trip (see Serializer<Animation>'s
// doc comment) -- playback progress is runtime-only.
void DrawInspector( Animation& inAnimation ) noexcept
{
    DrawTextField( "Clip Path", inAnimation.m_ClipPath );
    ImGui::DragFloat( "Frame Duration", &inAnimation.m_FrameDuration, 0.01f, 0.0f, 10.0f );
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

// Draws inName's section (if inEntity has a T) under its own ID scope, so
// two component types that happen to use the same field label (e.g. Sprite
// and Collider both have a "Layer") don't collide onto the same ImGui ID
// within the shared Inspector window -- SeparatorText is a label, not an ID
// scope, so without this every widget past the first "Layer" field would
// silently share state with it.
template<typename T>
void DrawSection( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity, char const* inName ) noexcept
{
    if ( auto r = inRegistry.GetComponent<T>( inEntity ) )
    {
        ImGui::PushID( inName );
        ImGui::SeparatorText( inName );
        DrawInspector( r.Value().get() );
        ImGui::PopID();
    }
}

}

void DrawEntityListPanel( asge::ecs::Registry& inRegistry, asge::ecs::Entity& ioSelected ) noexcept
{
    float const rightX = ImGui::GetIO().DisplaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
    ImGui::SetNextWindowPos( ImVec2( rightX, 95.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( kEditorPanelWidth, 160.0f ), ImGuiCond_FirstUseEver );

    ImGui::Begin( "Entities" );
    for ( auto entity : inRegistry.AllEntities() )
    {
        // "##<index>" suffix keeps each row's ImGui ID unique even when two
        // entities share the same (or default, unnamed) display label --
        // the same class of collision DrawSection's PushID guards against.
        std::string const label = GetEntityLabel( inRegistry, entity )
            + "##" + std::to_string( entity.m_Index );
        if ( ImGui::Selectable( label.c_str(), entity == ioSelected ) ) ioSelected = entity;
    }
    ImGui::End();
}

void DrawInspectorPanel( asge::ecs::Registry& inRegistry, asge::ecs::Entity inSelected ) noexcept
{
    if ( inSelected == asge::ecs::Entity::Null() ) return;

    float const rightX = ImGui::GetIO().DisplaySize.x - kEditorPanelWidth - kEditorPanelRightMargin;
    float const remainingHeight = ImGui::GetIO().DisplaySize.y - 265.0f - 20.0f; // fills down to a bottom margin
    ImGui::SetNextWindowPos( ImVec2( rightX, 265.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( kEditorPanelWidth, remainingHeight ), ImGuiCond_FirstUseEver );

    ImGui::Begin( "Inspector" );
    ImGui::Text( "%s", GetEntityLabel( inRegistry, inSelected ).c_str() );
    ImGui::Separator();

    DrawSection<Name>( inRegistry, inSelected, "Name" );
    DrawSection<Transform>( inRegistry, inSelected, "Transform" );
    DrawSection<Velocity>( inRegistry, inSelected, "Velocity" );
    DrawSection<Rigidbody>( inRegistry, inSelected, "Rigidbody" );
    DrawSection<Sprite>( inRegistry, inSelected, "Sprite" );
    DrawSection<Collider>( inRegistry, inSelected, "Collider" );
    DrawSection<Camera>( inRegistry, inSelected, "Camera" );
    DrawSection<AudioSource>( inRegistry, inSelected, "AudioSource" );
    DrawSection<Animation>( inRegistry, inSelected, "Animation" );
    DrawSection<PathFollow>( inRegistry, inSelected, "PathFollow" );

    ImGui::End();
}
