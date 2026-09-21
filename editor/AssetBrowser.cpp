#include "AssetBrowser.hpp"
#include "FileDialog.hpp"

#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Animation.hpp>

#include <SDL3/SDL_dialog.h>

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>

namespace
{
namespace fs = std::filesystem;
using asge::game::components::Sprite;
using asge::game::components::Animation;

// Explicitly imported via "Load Asset..." -- kept separate from (and merged
// with, at draw time) whatever's derived from the current scene's entities,
// since an empty/freshly-opened scene has nothing to derive from at all.
std::set<std::string> g_LoadedTextures;
std::set<std::string> g_LoadedAnimations;

// SDL_ShowOpenFileDialog's async result -- same FileDialogResult/
// DrainFileDialogResult plumbing main.cpp's scene dialogs use.
FileDialogResult g_LoadDialogResult;
std::string      g_PendingLoadRoot; // virtual root the picked file's path gets prefixed with
fs::path         g_PendingLoadDir;  // that root's real directory, for relative-path math once the dialog resolves

constexpr SDL_DialogFileFilter kAssetFilters[]{
    { "Images", "png;jpg;jpeg;bmp;tga;gif" },
    { "Animation clip (*.toml)", "toml" },
};

bool HasImageExtension( fs::path const& inPath ) noexcept
{
    static std::string const kImageExtensions[]{ ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif" };
    auto ext = inPath.extension().string();
    std::transform( ext.begin(), ext.end(), ext.begin(),
        []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
    return std::find( std::begin( kImageExtensions ), std::end( kImageExtensions ), ext )
        != std::end( kImageExtensions );
}

// FrameTable meta-files are plain ".toml", indistinguishable from a scene
// file by extension alone -- a cheap content sniff for the "[FrameTable]"
// table name tells them apart without a full TOML parse.
bool IsFrameTableFile( fs::path const& inPath ) noexcept
{
    if ( inPath.extension() != ".toml" ) return false;
    auto const content = asge::filesystem::ReadText( inPath );
    return content && content.Value().find( "[FrameTable]" ) != asge::str::String::npos;
}

void CollectUsedPaths(
    asge::ecs::Registry& inRegistry, std::set<std::string>& outTextures, std::set<std::string>& outAnimations ) noexcept
{
    for ( auto entity : inRegistry.AllEntities() )
    {
        if ( auto r = inRegistry.GetComponent<Sprite>( entity ); r && !r.Value().get().m_VirtualPath.empty() )
        {
            outTextures.insert( r.Value().get().m_VirtualPath );
        }
        if ( auto r = inRegistry.GetComponent<Animation>( entity ); r && !r.Value().get().m_ClipPath.empty() )
        {
            outAnimations.insert( r.Value().get().m_ClipPath );
        }
    }
}

std::set<std::string> MergedTextures( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures = g_LoadedTextures;
    std::set<std::string> animations; // discarded -- caller only wants textures
    CollectUsedPaths( inRegistry, textures, animations );
    return textures;
}

std::set<std::string> MergedAnimations( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures; // discarded -- caller only wants animations
    std::set<std::string> animations = g_LoadedAnimations;
    CollectUsedPaths( inRegistry, textures, animations );
    return animations;
}
}

std::vector<std::string> KnownTexturePaths( asge::ecs::Registry& inRegistry ) noexcept
{
    auto const textures = MergedTextures( inRegistry );
    return { textures.begin(), textures.end() };
}

std::vector<std::string> KnownAnimationPaths( asge::ecs::Registry& inRegistry ) noexcept
{
    auto const animations = MergedAnimations( inRegistry );
    return { animations.begin(), animations.end() };
}

void RegisterSceneAssets( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures;
    std::set<std::string> animations;
    CollectUsedPaths( inRegistry, textures, animations );
    g_LoadedTextures.insert( textures.begin(), textures.end() );
    g_LoadedAnimations.insert( animations.begin(), animations.end() );
}

AssetPick DrawAssetBrowserPanel(
    asge::ecs::Registry& inRegistry, asge::filesystem::VirtualFileSystem const& inVfs, SDL_Window* inWindow ) noexcept
{
    // Drained before drawing, same convention as every other panel with a
    // pending native dialog (VfsPanel, main.cpp's Save/Open).
    {
        std::string chosenFile;
        if ( DrainFileDialogResult( g_LoadDialogResult, chosenFile ) )
        {
            fs::path const picked = chosenFile;
            std::error_code ec;
            fs::path const relative = fs::relative( picked, g_PendingLoadDir, ec );
            if ( ec || relative.generic_string().rfind( "..", 0 ) == 0 )
            {
                LOG_ERROR( "\"", picked.string(), "\" is outside \"", g_PendingLoadRoot, "\"'s mounted directory" );
            }
            else
            {
                std::string const virtualPath = g_PendingLoadRoot + "/" + relative.generic_string();
                if ( HasImageExtension( picked ) )
                {
                    g_LoadedTextures.insert( virtualPath );
                    LOG_INFO( "Loaded texture ", virtualPath );
                }
                else if ( IsFrameTableFile( picked ) )
                {
                    g_LoadedAnimations.insert( virtualPath );
                    LOG_INFO( "Loaded animation clip ", virtualPath );
                }
                else
                {
                    LOG_WARNING( "\"", virtualPath, "\" is neither a recognized image nor a FrameTable clip" );
                }
            }
        }
    }

    std::set<std::string> const textures = MergedTextures( inRegistry );
    std::set<std::string> const animations = MergedAnimations( inRegistry );

    AssetPick pick;

    ImGui::SetNextWindowPos( ImVec2( 10.0f, 30.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 280.0f, 320.0f ), ImGuiCond_FirstUseEver );

    ImGui::Begin( "Assets" );

    if ( ImGui::Button( "Load Asset..." ) ) ImGui::OpenPopup( "LoadAssetMountPicker" );

    if ( ImGui::BeginPopup( "LoadAssetMountPicker" ) )
    {
        ImGui::TextDisabled( "Pick a mount to browse:" );
        ImGui::Separator();
        for ( auto const& mount : inVfs.ListMounts() )
        {
            std::string const label = mount.m_VirtualRoot + " -> " + mount.m_RealDirectory.string();
            if ( ImGui::Selectable( label.c_str() ) )
            {
                g_PendingLoadRoot = mount.m_VirtualRoot;
                g_PendingLoadDir = mount.m_RealDirectory;
                auto const defaultLocation = DialogDefaultLocation( g_PendingLoadDir );
                SDL_ShowOpenFileDialog(
                    OnFileDialogResult, &g_LoadDialogResult, inWindow,
                    kAssetFilters, 2, defaultLocation.c_str(), false );
                ImGui::CloseCurrentPopup();
            }
        }
        if ( inVfs.ListMounts().empty() )
        {
            ImGui::TextDisabled( "(no mounts -- add one in the Virtual File System panel)" );
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    if ( ImGui::TreeNodeEx( "Textures", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        for ( auto const& path : textures )
        {
            ImGui::PushID( path.c_str() );
            // AllowOverlap -- a plain Selectable's hit box otherwise spans
            // the full row and silently eats clicks meant for the "x"
            // button drawn on top of it further right (Selectable claims
            // the click before the button ever sees it without this flag).
            if ( ImGui::Selectable( path.c_str(), false, ImGuiSelectableFlags_AllowOverlap ) )
            {
                pick = { AssetPickKind::Texture, path };
            }
            // Only a manually "Load Asset..."-ed entry can be un-loaded --
            // one derived purely from scene usage has nothing here to
            // remove; it'd just reappear next frame from the entity itself.
            if ( g_LoadedTextures.count( path ) )
            {
                ImGui::SameLine( ImGui::GetWindowWidth() - 30.0f );
                if ( ImGui::SmallButton( "x" ) ) g_LoadedTextures.erase( path );
            }
            ImGui::PopID();
        }
        if ( textures.empty() ) ImGui::TextDisabled( "(none loaded yet)" );
        ImGui::TreePop();
    }

    if ( ImGui::TreeNodeEx( "Animation Clips", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        for ( auto const& path : animations )
        {
            ImGui::PushID( path.c_str() );
            if ( ImGui::Selectable( path.c_str(), false, ImGuiSelectableFlags_AllowOverlap ) )
            {
                pick = { AssetPickKind::Animation, path };
            }
            if ( g_LoadedAnimations.count( path ) )
            {
                ImGui::SameLine( ImGui::GetWindowWidth() - 30.0f );
                if ( ImGui::SmallButton( "x" ) ) g_LoadedAnimations.erase( path );
            }
            ImGui::PopID();
        }
        if ( animations.empty() ) ImGui::TextDisabled( "(none loaded yet)" );
        ImGui::TreePop();
    }

    ImGui::End();
    return pick;
}
