#include "AssetBrowser.hpp"
#include "FileDialog.hpp"

#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Game/Assets/AssetResolver.hpp>
#include <ASGE/Game/Assets/FrameTable.hpp>

#include <SDL3/SDL_dialog.h>

#include <imgui.h>

#include <filesystem>
#include <set>
#include <string>

namespace
{
namespace fs = std::filesystem;
using asge::game::asset::AssetKind;
using asge::game::asset::CollectAssetRefs;

// Explicitly imported via "Load Asset..." -- kept separate from (and merged
// with, at draw time) whatever's derived from the current scene's entities,
// since an empty/freshly-opened scene has nothing to derive from at all.
std::set<std::string> g_LoadedTextures;
std::set<std::string> g_LoadedAnimations;
std::set<std::string> g_LoadedAudio;

// SDL_ShowOpenFileDialog's async result -- same FileDialogResult/
// DrainFileDialogResult plumbing main.cpp's scene dialogs use.
FileDialogResult g_LoadDialogResult;
std::string      g_PendingLoadRoot; // virtual root the picked file's path gets prefixed with
fs::path         g_PendingLoadDir;  // that root's real directory, for relative-path math once the dialog resolves

constexpr SDL_DialogFileFilter kAssetFilters[]{
    { "Images", "png;jpg;jpeg;bmp;tga;gif" },
    { "Animation clip (*.toml)", "toml" },
    { "Audio", "wav;ogg" },
};

// media::AudioClip has no IsSupportedFile equivalent to Image's -- it
// dispatches strictly by exact-case ".wav"/".ogg" (see AudioClip::Load), so
// this mirrors that exact match rather than being more permissive than what
// actually decodes.
bool HasAudioExtension( fs::path const& inPath ) noexcept
{
    return inPath.extension() == ".wav" || inPath.extension() == ".ogg";
}

void CollectUsedPaths(
    asge::ecs::Registry& inRegistry,
    std::set<std::string>& outTextures, std::set<std::string>& outAnimations, std::set<std::string>& outAudio ) noexcept
{
    for ( auto const& ref : CollectAssetRefs( inRegistry ) )
    {
        switch ( ref.m_Kind )
        {
        case AssetKind::Texture:       outTextures.insert( ref.m_VirtualPath ); break;
        case AssetKind::AnimationClip: outAnimations.insert( ref.m_VirtualPath ); break;
        case AssetKind::AudioClip:     outAudio.insert( ref.m_VirtualPath ); break;
        }
    }
}

std::set<std::string> MergedTextures( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures = g_LoadedTextures;
    std::set<std::string> animations, audio; // discarded -- caller only wants textures
    CollectUsedPaths( inRegistry, textures, animations, audio );
    return textures;
}

std::set<std::string> MergedAnimations( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures, audio; // discarded -- caller only wants animations
    std::set<std::string> animations = g_LoadedAnimations;
    CollectUsedPaths( inRegistry, textures, animations, audio );
    return animations;
}

std::set<std::string> MergedAudio( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures, animations; // discarded -- caller only wants audio
    std::set<std::string> audio = g_LoadedAudio;
    CollectUsedPaths( inRegistry, textures, animations, audio );
    return audio;
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

std::vector<std::string> KnownAudioPaths( asge::ecs::Registry& inRegistry ) noexcept
{
    auto const audio = MergedAudio( inRegistry );
    return { audio.begin(), audio.end() };
}

void RegisterSceneAssets( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures, animations, audio;
    CollectUsedPaths( inRegistry, textures, animations, audio );
    g_LoadedTextures.insert( textures.begin(), textures.end() );
    g_LoadedAnimations.insert( animations.begin(), animations.end() );
    g_LoadedAudio.insert( audio.begin(), audio.end() );
}

void ImportAssets(
    std::vector<std::string> const& inTexturePaths,
    std::vector<std::string> const& inAnimationPaths,
    std::vector<std::string> const& inAudioPaths ) noexcept
{
    g_LoadedTextures.insert( inTexturePaths.begin(), inTexturePaths.end() );
    g_LoadedAnimations.insert( inAnimationPaths.begin(), inAnimationPaths.end() );
    g_LoadedAudio.insert( inAudioPaths.begin(), inAudioPaths.end() );
}

void ClearKnownAssets() noexcept
{
    g_LoadedTextures.clear();
    g_LoadedAnimations.clear();
    g_LoadedAudio.clear();
}

AssetPick DrawAssetBrowserPanel(
    asge::ecs::Registry& inRegistry, asge::filesystem::VirtualFileSystem const& inVfs, SDL_Window* inWindow,
    bool inHasProject ) noexcept
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
                if ( asge::media::Image::IsSupportedFile( picked ) )
                {
                    g_LoadedTextures.insert( virtualPath );
                    LOG_INFO( "Loaded texture ", virtualPath );
                }
                else if ( asge::game::asset::FrameTable::IsFrameTable( picked ) )
                {
                    // Beyond just being a [FrameTable] table, a clip
                    // imported this way must also name the texture it
                    // slices (m_OriginAsset) -- one hand-authored or
                    // produced elsewhere without it is rejected rather than
                    // imported half-identified; one made by this editor's
                    // own "Create Clip" always has it set.
                    auto loadedClip = asge::game::asset::FrameTable::Load( picked );
                    if ( !loadedClip || loadedClip.Value().m_OriginAsset.empty() )
                    {
                        LOG_WARNING(
                            "\"", virtualPath, "\" is a FrameTable clip with no m_OriginAsset -- rejected" );
                    }
                    else
                    {
                        std::string const originPath( loadedClip.Value().m_OriginAsset );
                        g_LoadedAnimations.insert( virtualPath );
                        g_LoadedTextures.insert( originPath );
                        LOG_INFO( "Loaded animation clip ", virtualPath, " (origin: ", originPath, ")" );
                    }
                }
                else if ( HasAudioExtension( picked ) )
                {
                    g_LoadedAudio.insert( virtualPath );
                    LOG_INFO( "Loaded audio clip ", virtualPath );
                }
                else
                {
                    LOG_WARNING( "\"", virtualPath, "\" is not a recognized image, FrameTable clip, or audio file" );
                }
            }
        }
    }

    std::set<std::string> const textures = MergedTextures( inRegistry );
    std::set<std::string> const animations = MergedAnimations( inRegistry );
    std::set<std::string> const audio = MergedAudio( inRegistry );

    // Whether an entity's Sprite/Animation/AudioSource actually references a
    // path right now -- checked before the "x" is allowed to remove it. The
    // already-loaded ITexture/clip an entity is using stays cached regardless
    // of this panel's own bookkeeping, so silently un-importing a path still
    // in use wouldn't stop it rendering/playing; it would just make the
    // panel lie about what's actually bound.
    std::set<std::string> usedTextures, usedAnimations, usedAudio;
    CollectUsedPaths( inRegistry, usedTextures, usedAnimations, usedAudio );

    AssetPick pick;

    // FirstUseEver, not Always -- (10, 30) is a constant, not derived from
    // DisplaySize, so there's nothing a resize could invalidate here; unlike
    // the right-edge panels (Scene/Entities/Inspector), forcing this every
    // frame bought nothing but made the panel undraggable.
    ImGui::SetNextWindowPos( ImVec2( 10.0f, 30.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 280.0f, 320.0f ), ImGuiCond_FirstUseEver );

    ImGui::Begin( "Assets" );

    if ( !inHasProject ) ImGui::BeginDisabled();
    if ( ImGui::Button( "Load Asset..." ) ) ImGui::OpenPopup( "LoadAssetMountPicker" );
    if ( !inHasProject ) ImGui::EndDisabled();

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
                    kAssetFilters, 3, defaultLocation.c_str(), false );
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
                if ( ImGui::SmallButton( "x" ) )
                {
                    if ( usedTextures.count( path ) )
                    {
                        LOG_WARNING( "One or more entities are currently using \"", path, "\" -- not removed" );
                    }
                    else
                    {
                        g_LoadedTextures.erase( path );
                    }
                }
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
                if ( ImGui::SmallButton( "x" ) )
                {
                    if ( usedAnimations.count( path ) )
                    {
                        LOG_WARNING( "One or more entities are currently using \"", path, "\" -- not removed" );
                    }
                    else
                    {
                        g_LoadedAnimations.erase( path );
                    }
                }
            }
            ImGui::PopID();
        }
        if ( animations.empty() ) ImGui::TextDisabled( "(none loaded yet)" );
        ImGui::TreePop();
    }

    if ( ImGui::TreeNodeEx( "Audio Clips", ImGuiTreeNodeFlags_DefaultOpen ) )
    {
        for ( auto const& path : audio )
        {
            ImGui::PushID( path.c_str() );
            if ( ImGui::Selectable( path.c_str(), false, ImGuiSelectableFlags_AllowOverlap ) )
            {
                pick = { AssetPickKind::Audio, path };
            }
            if ( g_LoadedAudio.count( path ) )
            {
                ImGui::SameLine( ImGui::GetWindowWidth() - 30.0f );
                if ( ImGui::SmallButton( "x" ) )
                {
                    if ( usedAudio.count( path ) )
                    {
                        LOG_WARNING( "One or more entities are currently using \"", path, "\" -- not removed" );
                    }
                    else
                    {
                        g_LoadedAudio.erase( path );
                    }
                }
            }
            ImGui::PopID();
        }
        if ( audio.empty() ) ImGui::TextDisabled( "(none loaded yet)" );
        ImGui::TreePop();
    }

    ImGui::End();
    return pick;
}
