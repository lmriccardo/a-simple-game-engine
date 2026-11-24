#include "PrefabEditor.hpp"

#include "AssetBrowser.hpp"
#include "Inspector.hpp"

#include <ASGE/Core/ECS/Hierarchy.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Scene/SceneSerializer.hpp>

#include <imgui.h>

#include <memory>
#include <string>
#include <vector>

namespace
{

// The prefab being edited. Statics, like the other editor panels, since only one modal exists.
struct PrefabEditorState
{
    std::unique_ptr<asge::ecs::Registry> m_Registry;                       // the prefab's entities, apart from the scene
    asge::ecs::Entity                    m_Root     = asge::ecs::Entity::Null();
    asge::ecs::Entity                    m_Selected = asge::ecs::Entity::Null();
    std::string                          m_Path;                           // virtual path of the prefab file
    bool                                 m_Request  = false;               // open the modal on the next draw
    WaypointEditState                    m_Waypoints;
    ColliderDrawState                    m_Collider;
};

PrefabEditorState g_State;

void Close() noexcept
{
    g_State.m_Registry.reset();
    g_State.m_Root = g_State.m_Selected = asge::ecs::Entity::Null();
    g_State.m_Waypoints = {};
    g_State.m_Collider = {};
}

void DrawTree( asge::ecs::Registry& inRegistry, asge::ecs::Entity inEntity ) noexcept
{
    std::vector<asge::ecs::Entity> children;
    asge::ecs::components::ForEachChild( inRegistry, inEntity, [&]( asge::ecs::Entity inChild )
    { children.push_back( inChild ); } );

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen
        | ImGuiTreeNodeFlags_SpanAvailWidth;
    if ( children.empty() ) flags |= ImGuiTreeNodeFlags_Leaf;
    if ( g_State.m_Selected == inEntity ) flags |= ImGuiTreeNodeFlags_Selected;

    std::string const label = GetEntityLabel( inRegistry, inEntity ) + "##" + std::to_string( inEntity.m_Index );
    bool const open = ImGui::TreeNodeEx( label.c_str(), flags );
    if ( ImGui::IsItemClicked() ) g_State.m_Selected = inEntity;
    if ( !open ) return;

    for ( auto child : children ) DrawTree( inRegistry, child );
    ImGui::TreePop();
}

// Writes the prefab back and reloads the asset. True if the file was written.
bool Save(
    asge::filesystem::VirtualFileSystem const& inVfs, asge::game::asset::AssetManager& inAssets,
    asge::ecs::Registry& inSceneRegistry ) noexcept
{
    auto const real = inVfs.Resolve( g_State.m_Path );
    if ( !real ) { real.LogError(); return false; }

    if ( auto const saved = asge::game::scene::SceneSerializer( inVfs ).SavePrefab(
             *g_State.m_Registry, g_State.m_Root, real.Value() ); !saved )
    {
        saved.LogError();
        return false;
    }

    inAssets.ReloadAsset( g_State.m_Path, inSceneRegistry );
    LOG_INFO( "Saved prefab ", g_State.m_Path );
    return true;
}

} // namespace

void OpenPrefabEditor(
    asge::filesystem::VirtualFileSystem const& inVfs, asge::game::asset::AssetManager& inAssets,
    asge::video::IRenderer& inRenderer, std::string const& inVirtualPath ) noexcept
{
    auto prefab = inAssets.GetScenePrefab( inVirtualPath );
    if ( !prefab ) { prefab.LogError(); return; }

    auto registry = std::make_unique<asge::ecs::Registry>();
    auto root = asge::game::scene::SceneSerializer( inVfs ).Instantiate( *registry, prefab.Value()->Get() );
    if ( !root ) { root.LogError(); return; }

    inAssets.ResolveAssets( *registry, inRenderer );

    Close();
    g_State.m_Registry = std::move( registry );
    g_State.m_Root = g_State.m_Selected = root.Value();
    g_State.m_Path = inVirtualPath;
    g_State.m_Request = true;
}

void DrawPrefabEditor(
    asge::filesystem::VirtualFileSystem const& inVfs, asge::game::asset::AssetManager& inAssets,
    asge::video::IRenderer& inRenderer, asge::ecs::Registry& inSceneRegistry ) noexcept
{
    if ( !g_State.m_Registry ) return;

    constexpr char kPopup[] = "Edit Prefab";
    if ( g_State.m_Request )
    {
        ImGui::OpenPopup( kPopup );
        g_State.m_Request = false;
    }

    ImGui::SetNextWindowSize( ImVec2( 880.0f, 540.0f ), ImGuiCond_Appearing );
    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f ) );

    bool open = true;
    bool const shown = ImGui::BeginPopupModal( kPopup, &open );
    if ( !open ) Close(); // closed with the title-bar X
    if ( !shown ) return;

    auto& registry = *g_State.m_Registry;
    float const footer = ImGui::GetFrameHeightWithSpacing() + 4.0f;

    ImGui::TextDisabled( "%s", g_State.m_Path.c_str() );

    ImGui::BeginChild( "##prefabTree", ImVec2( 280.0f, -footer ), ImGuiChildFlags_Borders );
    DrawTree( registry, g_State.m_Root );
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild( "##prefabInspector", ImVec2( 0.0f, -footer ), ImGuiChildFlags_Borders );
    auto const inspected = DrawInspectorPanel(
        registry, g_State.m_Selected,
        KnownTexturePaths( registry ), KnownAudioPaths( registry ), KnownFontPaths( registry ),
        g_State.m_Waypoints, g_State.m_Collider, true );
    if ( g_State.m_Selected == asge::ecs::Entity::Null() ) ImGui::TextDisabled( "Select an entity in the tree." );
    ImGui::EndChild();

    switch ( inspected.m_Action )
    {
    case EntityAction::Delete:
        if ( g_State.m_Selected == g_State.m_Root )
        {
            LOG_WARNING( "The root entity of a prefab cannot be deleted" );
        }
        else
        {
            asge::ecs::components::DestroyEntityGraph( registry, g_State.m_Selected );
            g_State.m_Selected = g_State.m_Root;
        }
        break;
    case EntityAction::ComponentsChanged:
        inAssets.ResolveAssets( registry, inRenderer );
        break;
    default:
        break;
    }

    if ( ImGui::Button( "Add Child" ) && g_State.m_Selected != asge::ecs::Entity::Null() )
    {
        if ( auto created = registry.CreateEntity() )
        {
            registry.AddComponent<asge::game::components::Transform>( created.Value(), asge::game::components::Transform{} );
            asge::ecs::components::AttachChild( registry, g_State.m_Selected, created.Value() );
            g_State.m_Selected = created.Value();
        }
        else created.LogError();
    }

    ImGui::SameLine( ImGui::GetWindowWidth() - 190.0f );
    bool close = false;
    if ( ImGui::Button( "Save", ImVec2( 80.0f, 0.0f ) ) ) close = Save( inVfs, inAssets, inSceneRegistry );
    ImGui::SameLine();
    if ( ImGui::Button( "Cancel", ImVec2( 80.0f, 0.0f ) ) ) close = true;

    if ( close ) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    if ( close ) Close();
}
