#include "VfsPanel.hpp"
#include "FileDialog.hpp"
#include "AssetBrowser.hpp"

#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/AudioSource.hpp>

#include <SDL3/SDL_dialog.h>

#include <imgui.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
using asge::game::components::Sprite;
using asge::game::components::Animation;
using asge::game::components::AudioSource;

// SDL_ShowOpenFolderDialog shares SDL_ShowSaveFileDialog/SDL_ShowOpenFileDialog's
// async callback shape, so the same FileDialogResult/DrainFileDialogResult
// main.cpp's scene dialogs use covers this one too -- no new plumbing.
FileDialogResult g_MountDialogResult;
std::string      g_PendingMountName; // set right before opening the dialog, consumed once it resolves

char g_NewMountNameBuf[64] = "";

// Every distinct, non-empty virtual root the current scene's asset-owning
// components reference. Sprite/Animation/AudioSource are the only
// SerializableComponents types with a virtual-path field (see
// AssetResolver.hpp) -- Path builds its own runtime path from
// waypoints, nothing else owns an asset.
std::set<std::string> ReferencedRoots( asge::ecs::Registry& inRegistry ) noexcept
{
    using asge::filesystem::VirtualFileSystem;

    std::set<std::string> roots;
    for ( auto entity : inRegistry.AllEntities() )
    {
        if ( auto r = inRegistry.GetComponent<Sprite>( entity ); r && !r.Value().get().m_VirtualPath.empty() )
        {
            roots.insert( VirtualFileSystem::SplitRoot( r.Value().get().m_VirtualPath ).m_Root );
        }
        if ( auto r = inRegistry.GetComponent<Animation>( entity ); r && !r.Value().get().m_ClipPath.empty() )
        {
            roots.insert( VirtualFileSystem::SplitRoot( r.Value().get().m_ClipPath ).m_Root );
        }
        if ( auto r = inRegistry.GetComponent<AudioSource>( entity ); r && !r.Value().get().m_VirtualClipPath.empty() )
        {
            roots.insert( VirtualFileSystem::SplitRoot( r.Value().get().m_VirtualClipPath ).m_Root );
        }
    }
    return roots;
}

void RequestMountFolder( std::string const& inName, SDL_Window* inWindow ) noexcept
{
    g_PendingMountName = inName;
    SDL_ShowOpenFolderDialog( OnFileDialogResult, &g_MountDialogResult, inWindow, nullptr, false );
}

// The same virtual root legitimately mapping to more than one real
// directory is intentional, not a mistake to correct here -- Resolve()
// tries every mount matching a given root in registration order until one
// actually has the requested file, an overlay/search-path style setup
// (e.g. a base "assets" mount plus a DLC/mod folder layered under the same
// name). So adding a mount just adds it; grouping same-named entries for
// display is DrawVfsContents's job below, not this function's.
void AddMount( asge::filesystem::VirtualFileSystem& inVfs, std::string const& inName, std::string const& inRealDir ) noexcept
{
    if ( auto const r = inVfs.Mount( inName, inRealDir ); !r ) r.LogError();
    else LOG_INFO( "Mounted \"", inName, "\" -> ", inRealDir );
}

// Removing a mount can silently strand every asset that depended on it --
// nothing else re-checks resolution on its own (ResolveAssets only ever
// runs on specific triggers, and none of them is "a mount just went away"),
// so this is the one place that actually notices. Checked against every
// texture/animation path the editor currently knows about (scene usage +
// "Load Asset..." imports, i.e. exactly AssetBrowser's own listing) rather
// than only what's already resolved -- an unresolved Sprite wouldn't have
// logged anything the first time either, so silence here would compound it.
void LogNowUnresolvable( asge::filesystem::VirtualFileSystem const& inVfs, asge::ecs::Registry& inRegistry ) noexcept
{
    for ( auto const& path : KnownTexturePaths( inRegistry ) )
    {
        if ( !inVfs.Resolve( path ) ) LOG_ERROR( "\"", path, "\" can no longer be resolved (mount removed)" );
    }
    for ( auto const& path : KnownAnimationPaths( inRegistry ) )
    {
        if ( !inVfs.Resolve( path ) ) LOG_ERROR( "\"", path, "\" can no longer be resolved (mount removed)" );
    }
}

constexpr float kRowHeight = 30.0f; // every mountpoint row is this tall, so the list reads as evenly spaced

// The right-click menu of a mounted root: one "Unmount" per real directory
// mapped to it (the same root can map to several -- see AddMount).
void DrawMountContextMenu(
    asge::filesystem::VirtualFileSystem& inVfs, asge::ecs::Registry& inRegistry,
    std::string const& inRoot, std::vector<std::string> const& inDirs ) noexcept
{
    if ( !ImGui::BeginPopupContextItem() ) return;

    for ( auto const& dir : inDirs )
    {
        std::string const label = "Unmount " + dir;
        if ( ImGui::MenuItem( label.c_str() ) )
        {
            if ( auto const r = inVfs.Unmount( inRoot, dir ); !r ) r.LogError();
            else
            {
                LOG_INFO( "Unmounted \"", inRoot, "\" -> ", dir );
                LogNowUnresolvable( inVfs, inRegistry );
            }
        }
    }
    ImGui::EndPopup();
}
}

void DrawMountList(
    asge::filesystem::VirtualFileSystem& inVfs, asge::ecs::Registry& inRegistry,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    SDL_Window* inWindow, bool inHasProject, AssetExplorerState& ioState ) noexcept
{
    // Drained before drawing -- whichever mount name was pending (from the
    // "Add mount" popup or a Missing row's "Set...") gets bound to whatever
    // real directory the user just picked.
    {
        std::string chosenDir;
        if ( DrainFileDialogResult( g_MountDialogResult, chosenDir ) && !g_PendingMountName.empty() )
        {
            AddMount( inVfs, g_PendingMountName, chosenDir );
            inAssets.ResolveAssets( inRegistry, inRenderer );
            g_PendingMountName.clear();
        }
    }

    // Grouped by virtual root, not one row per (name, dir) pair -- the same
    // root can legitimately map to several real directories (see AddMount's
    // own comment), and showing "assets -> X" / "assets -> Y" as two
    // unrelated-looking rows reads as a mistake rather than the overlay it is.
    std::vector<std::string> order; // first-seen order, for stable display
    std::map<std::string, std::vector<std::string>> byRoot;
    for ( auto const& mount : inVfs.ListMounts() )
    {
        auto const [it, inserted] = byRoot.try_emplace( mount.m_VirtualRoot );
        if ( inserted ) order.push_back( mount.m_VirtualRoot );
        it->second.push_back( mount.m_RealDirectory.string() );
    }

    // Roots this scene's assets use that nothing mounts: shown after the
    // mounted ones, flagged, and bound to a folder with "Set...".
    std::set<std::string> missing;
    for ( auto const& root : ReferencedRoots( inRegistry ) )
    {
        if ( !inVfs.IsMounted( root ) ) missing.insert( root );
    }

    // The selected root can vanish (unmounted, or a different project): fall
    // back to the first one, or to none.
    if ( !byRoot.contains( ioState.m_Root ) )
    {
        ioState.m_Root = order.empty() ? std::string{} : order.front();
        ioState.m_Folder.clear();
    }

    ImGui::BeginChild( "##mountRows", ImVec2( 0.0f, -ImGui::GetFrameHeightWithSpacing() ) );
    for ( auto const& root : order )
    {
        ImGui::PushID( root.c_str() );
        bool const selected = root == ioState.m_Root;
        if ( ImGui::Selectable( root.c_str(), selected, 0, ImVec2( 0.0f, kRowHeight ) ) && !selected )
        {
            ioState.m_Root = root;
            ioState.m_Folder.clear();
        }
        if ( ImGui::IsItemHovered() )
        {
            std::string dirs;
            for ( auto const& dir : byRoot[root] ) dirs += ( dirs.empty() ? "" : "\n" ) + dir;
            ImGui::SetTooltip( "%s", dirs.c_str() );
        }
        DrawMountContextMenu( inVfs, inRegistry, root, byRoot[root] );
        ImGui::PopID();
    }

    for ( auto const& root : missing )
    {
        ImGui::PushID( root.c_str() );
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.8f, 0.2f, 1.0f ) );
        ImGui::Selectable( root.c_str(), false, ImGuiSelectableFlags_Disabled, ImVec2( 0.0f, kRowHeight ) );
        ImGui::PopStyleColor();
        // A disabled Selectable swallows hover/click, so the "Set..." button sits on the row's right.
        ImGui::SameLine( ImGui::GetWindowWidth() - 70.0f );
        if ( ImGui::SmallButton( "Set..." ) ) RequestMountFolder( root, inWindow );
        if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( "Used by this scene but not mounted. Pick its folder." );
        ImGui::PopID();
    }
    ImGui::EndChild();

    if ( !inHasProject ) ImGui::BeginDisabled();
    if ( ImGui::Button( "+ Add mount" ) ) ImGui::OpenPopup( "##addMount" );
    if ( !inHasProject ) ImGui::EndDisabled();

    if ( ImGui::BeginPopup( "##addMount" ) )
    {
        ImGui::TextDisabled( "Mount name:" );
        ImGui::SetNextItemWidth( 160.0f );
        ImGui::InputText( "##MountName", g_NewMountNameBuf, sizeof( g_NewMountNameBuf ) );
        ImGui::SameLine();
        if ( ImGui::Button( "Browse..." ) && g_NewMountNameBuf[0] != '\0' )
        {
            RequestMountFolder( g_NewMountNameBuf, inWindow );
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
