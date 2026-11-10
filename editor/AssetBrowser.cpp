#include "AssetBrowser.hpp"
#include "FileDialog.hpp"
#include "AssetTree.hpp"
#include "Inspector.hpp" // GetEntityLabel, for the "Attach To" submenu's entity list

#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Game/Assets/AssetResolver.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Assets/FrameTable.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>

#include <SDL3/SDL_dialog.h>

#include <imgui.h>

#include <filesystem>
#include <algorithm>
#include <iterator>
#include <map>
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
std::set<std::string> g_LoadedFonts;

// SDL_ShowOpenFileDialog's async result -- same FileDialogResult/
// DrainFileDialogResult plumbing main.cpp's scene dialogs use.
FileDialogResult g_LoadDialogResult;
std::string      g_PendingLoadRoot; // virtual root the picked file's path gets prefixed with
fs::path         g_PendingLoadDir;  // that root's real directory, for relative-path math once the dialog resolves

constexpr SDL_DialogFileFilter kAssetFilters[]{
    { "Images", "png;jpg;jpeg;bmp;tga;gif" },
    { "Animation clip (*.toml)", "toml" },
    { "Audio", "wav;ogg" },
    { "Font", "ttf" },
};

// media::AudioClip has no IsSupportedFile equivalent to Image's -- it
// dispatches strictly by exact-case ".wav"/".ogg" (see AudioClip::Load), so
// this mirrors that exact match rather than being more permissive than what
// actually decodes.
bool HasAudioExtension( fs::path const& inPath ) noexcept
{
    return inPath.extension() == ".wav" || inPath.extension() == ".ogg";
}

// media::Font::Load's own doc comment documents only ".ttf" (stb_truetype
// under the hood) -- mirrored exactly rather than guessing at broader
// TrueType/OpenType support it never claimed.
bool HasFontExtension( fs::path const& inPath ) noexcept
{
    return inPath.extension() == ".ttf";
}

void CollectUsedPaths(
    asge::ecs::Registry& inRegistry,
    std::set<std::string>& outTextures, std::set<std::string>& outAnimations, std::set<std::string>& outAudio,
    std::set<std::string>& outFonts ) noexcept
{
    for ( auto const& ref : CollectAssetRefs( inRegistry ) )
    {
        switch ( ref.m_Kind )
        {
        case AssetKind::Texture:       outTextures.insert( ref.m_VirtualPath ); break;
        case AssetKind::AnimationClip: outAnimations.insert( ref.m_VirtualPath ); break;
        case AssetKind::AnimationClipSet: break; // not browsed in the editor
        case AssetKind::AudioClip:     outAudio.insert( ref.m_VirtualPath ); break;
        case AssetKind::Font:          outFonts.insert( ref.m_VirtualPath ); break;
        }
    }
}

std::set<std::string> MergedTextures( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures = g_LoadedTextures;
    std::set<std::string> animations, audio, fonts; // discarded -- caller only wants textures
    CollectUsedPaths( inRegistry, textures, animations, audio, fonts );
    return textures;
}

std::set<std::string> MergedAnimations( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures, audio, fonts; // discarded -- caller only wants animations
    std::set<std::string> animations = g_LoadedAnimations;
    CollectUsedPaths( inRegistry, textures, animations, audio, fonts );
    return animations;
}

std::set<std::string> MergedAudio( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures, animations, fonts; // discarded -- caller only wants audio
    std::set<std::string> audio = g_LoadedAudio;
    CollectUsedPaths( inRegistry, textures, animations, audio, fonts );
    return audio;
}

std::set<std::string> MergedFonts( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures, animations, audio; // discarded -- caller only wants fonts
    std::set<std::string> fonts = g_LoadedFonts;
    CollectUsedPaths( inRegistry, textures, animations, audio, fonts );
    return fonts;
}

// Phase 14: one asset row's right-click menu -- Create Entity/Attach To for
// every kind, plus Create Clip for a texture row only (inAllowCreateClip).
// Only ever reports one action into ioResult per frame, same "one user
// gesture" assumption DrawEntityListPanel's own context menu makes; actually
// creating the entity/attaching the component/opening the modal is deferred
// to the caller (main.cpp for the first two, DrawAssetInspectorPanel's own
// inOpenCreateClip for the third) rather than done here.
void DrawAssetContextMenu(
    asge::ecs::Registry& inRegistry, AssetPickKind inKind, std::string const& inPath,
    bool inAllowCreateClip, bool inShowActions, bool inHasProject,
    std::set<std::string>& ioLoaded, std::set<std::string> const& inUsed, AssetBrowserResult& ioResult ) noexcept
{
    bool const removable = ioLoaded.count( inPath ) > 0;
    if ( !inShowActions && !removable ) return; // nothing to offer, so no empty popup
    if ( !ImGui::BeginPopupContextItem() ) return;

    if ( inShowActions )
    {
    ImGui::BeginDisabled( !inHasProject );
    if ( ImGui::MenuItem( "Create Entity" ) )
    {
        ioResult.m_ContextAction = AssetContextAction::CreateEntity;
        ioResult.m_ContextKind = inKind;
        ioResult.m_ContextPath = inPath;
    }
    if ( ImGui::BeginMenu( "Attach To" ) )
    {
        auto const entities = inRegistry.AllEntities();
        for ( auto entity : entities )
        {
            std::string const label = GetEntityLabel( inRegistry, entity ) + "##" + std::to_string( entity.m_Index );
            if ( ImGui::MenuItem( label.c_str() ) )
            {
                ioResult.m_ContextAction = AssetContextAction::AttachTo;
                ioResult.m_ContextKind = inKind;
                ioResult.m_ContextPath = inPath;
                ioResult.m_ContextTarget = entity;
            }
        }
        if ( entities.empty() ) ImGui::TextDisabled( "(no entities)" );
        ImGui::EndMenu();
    }
    ImGui::EndDisabled();

    // Not gated on inHasProject -- writes a clip file next to the texture
    // regardless of any active project/scene, same as the inline button
    // this replaces (see DrawAssetInspectorPanel's own inOpenCreateClip).
    if ( inAllowCreateClip && ImGui::MenuItem( "Create Clip" ) )
    {
        ioResult.m_Pick = { AssetPickKind::Texture, inPath };
        ioResult.m_OpenCreateClip = true;
    }

    if ( inAllowCreateClip && ImGui::MenuItem( "Reload Asset" ) )
    {
        ioResult.m_ContextAction = AssetContextAction::ReloadTexture;
        ioResult.m_ContextKind = inKind;
        ioResult.m_ContextPath = inPath;
    }
    }

    // Only a manually "Load Asset..."-ed entry can be un-loaded -- one
    // derived purely from scene usage has nothing here to remove; it'd just
    // reappear next frame from the entity itself.
    if ( removable )
    {
        if ( inShowActions ) ImGui::Separator();
        if ( ImGui::MenuItem( "Remove from list" ) )
        {
            if ( inUsed.count( inPath ) )
            {
                LOG_WARNING( "One or more entities are currently using \"", inPath, "\" -- not removed" );
            }
            else
            {
                ioLoaded.erase( inPath );
            }
        }
    }

    ImGui::EndPopup();
}

// One explorer cell: a square icon with the item's name under it.
constexpr float kCellWidth = 88.0f;
constexpr float kIconSize = 56.0f;

enum class ExplorerItem { Folder, Texture, Animation, Audio, Font };

// What drawing one explorer cell needs besides the cell itself.
struct ExplorerContext
{
    asge::game::asset::AssetManager& m_Assets;
    asge::video::IRenderer&          m_Renderer;
};

// Texture paths whose thumbnail failed to load, mapped to the frame at which to try again. A
// path that does not resolve (missing mount) would otherwise be re-read from disk every frame;
// retrying now and then lets it appear once the mount is fixed.
std::map<std::string, int> g_ThumbnailRetryAt;
constexpr int kThumbnailRetryFrames = 120;

// Shortens inText with "..." until it fits inMaxWidth.
std::string FitText( std::string const& inText, float inMaxWidth ) noexcept
{
    if ( ImGui::CalcTextSize( inText.c_str() ).x <= inMaxWidth ) return inText;

    std::string text = inText;
    while ( !text.empty() && ImGui::CalcTextSize( ( text + "..." ).c_str() ).x > inMaxWidth ) text.pop_back();
    return text + "...";
}

void DrawFolderIcon( ImDrawList* inList, ImVec2 inMin ) noexcept
{
    float const s = kIconSize;
    inList->AddRectFilled(
        ImVec2( inMin.x, inMin.y + s * 0.14f ), ImVec2( inMin.x + s * 0.42f, inMin.y + s * 0.36f ),
        IM_COL32( 196, 150, 48, 255 ), 3.0f );
    inList->AddRectFilled(
        ImVec2( inMin.x, inMin.y + s * 0.26f ), ImVec2( inMin.x + s, inMin.y + s * 0.88f ),
        IM_COL32( 232, 188, 72, 255 ), 4.0f );
}

// A film strip with a play triangle: the animation clip icon.
void DrawAnimationIcon( ImDrawList* inList, ImVec2 inMin ) noexcept
{
    float const s = kIconSize;
    ImVec2 const max( inMin.x + s, inMin.y + s );
    inList->AddRectFilled( inMin, max, IM_COL32( 58, 70, 118, 255 ), 4.0f );

    // Perforations down both long edges.
    for ( int i = 0; i < 4; ++i )
    {
        float const y = inMin.y + s * ( 0.1f + 0.22f * static_cast<float>( i ) );
        inList->AddRectFilled( ImVec2( inMin.x + s * 0.05f, y ), ImVec2( inMin.x + s * 0.17f, y + s * 0.12f ), IM_COL32( 200, 205, 225, 255 ) );
        inList->AddRectFilled( ImVec2( max.x - s * 0.17f, y ), ImVec2( max.x - s * 0.05f, y + s * 0.12f ), IM_COL32( 200, 205, 225, 255 ) );
    }
    inList->AddTriangleFilled(
        ImVec2( inMin.x + s * 0.38f, inMin.y + s * 0.28f ), ImVec2( inMin.x + s * 0.38f, inMin.y + s * 0.72f ),
        ImVec2( inMin.x + s * 0.68f, inMin.y + s * 0.5f ), IM_COL32( 235, 238, 250, 255 ) );
}

// A musical note: the audio clip icon.
void DrawAudioIcon( ImDrawList* inList, ImVec2 inMin ) noexcept
{
    float const s = kIconSize;
    ImU32 const note = IM_COL32( 225, 245, 235, 255 );
    inList->AddRectFilled( inMin, ImVec2( inMin.x + s, inMin.y + s ), IM_COL32( 48, 112, 92, 255 ), 4.0f );
    inList->AddCircleFilled( ImVec2( inMin.x + s * 0.4f, inMin.y + s * 0.7f ), s * 0.14f, note );
    inList->AddLine( ImVec2( inMin.x + s * 0.53f, inMin.y + s * 0.7f ), ImVec2( inMin.x + s * 0.53f, inMin.y + s * 0.22f ), note, 3.0f );
    inList->AddLine( ImVec2( inMin.x + s * 0.53f, inMin.y + s * 0.22f ), ImVec2( inMin.x + s * 0.78f, inMin.y + s * 0.36f ), note, 3.0f );
}

// "Aa": the font icon.
void DrawFontIcon( ImDrawList* inList, ImVec2 inMin ) noexcept
{
    float const s = kIconSize;
    inList->AddRectFilled( inMin, ImVec2( inMin.x + s, inMin.y + s ), IM_COL32( 96, 72, 128, 255 ), 4.0f );

    float const size = s * 0.5f;
    ImFont* const font = ImGui::GetFont();
    ImVec2 const text = font->CalcTextSizeA( size, FLT_MAX, 0.0f, "Aa" );
    inList->AddText(
        font, size, ImVec2( inMin.x + ( s - text.x ) * 0.5f, inMin.y + ( s - text.y ) * 0.5f ),
        IM_COL32( 240, 232, 250, 255 ), "Aa" );
}

// The texture itself, shrunk to fit the icon square and centered in it. False if it could not
// be loaded, in which case the caller draws a placeholder.
bool DrawTextureThumbnail( ImDrawList* inList, ImVec2 inMin, std::string const& inPath, ExplorerContext& ioCtx ) noexcept
{
    int const frame = ImGui::GetFrameCount();
    if ( auto const it = g_ThumbnailRetryAt.find( inPath ); it != g_ThumbnailRetryAt.end() )
    {
        if ( frame < it->second ) return false;
        g_ThumbnailRetryAt.erase( it );
    }

    // GetTexture is path-cached by AssetManager, so this is only a lookup after the first call.
    auto loaded = ioCtx.m_Assets.GetTexture( inPath, ioCtx.m_Renderer );
    if ( !loaded )
    {
        g_ThumbnailRetryAt[inPath] = frame + kThumbnailRetryFrames;
        return false;
    }

    auto* const texture = loaded.Value();
    auto const size = texture->Size();
    float const largest = static_cast<float>( std::max( size.x(), size.y() ) );
    if ( largest <= 0.0f ) return false;

    float const scale = kIconSize / largest;
    float const w = static_cast<float>( size.x() ) * scale;
    float const h = static_cast<float>( size.y() ) * scale;
    ImVec2 const a( inMin.x + ( kIconSize - w ) * 0.5f, inMin.y + ( kIconSize - h ) * 0.5f );

    inList->AddRectFilled( inMin, ImVec2( inMin.x + kIconSize, inMin.y + kIconSize ), IM_COL32( 36, 36, 40, 255 ), 4.0f );
    inList->AddImage( texture->NativeHandle(), a, ImVec2( a.x + w, a.y + h ) );
    return true;
}

// Stand-in for a texture that could not be loaded (or is not on screen yet): a dim square with a cross.
void DrawMissingTextureIcon( ImDrawList* inList, ImVec2 inMin ) noexcept
{
    ImVec2 const max( inMin.x + kIconSize, inMin.y + kIconSize );
    inList->AddRectFilled( inMin, max, IM_COL32( 52, 52, 58, 255 ), 4.0f );
    inList->AddLine( ImVec2( inMin.x + 12.0f, inMin.y + 12.0f ), ImVec2( max.x - 12.0f, max.y - 12.0f ), IM_COL32( 150, 90, 90, 255 ), 2.0f );
    inList->AddLine( ImVec2( max.x - 12.0f, inMin.y + 12.0f ), ImVec2( inMin.x + 12.0f, max.y - 12.0f ), IM_COL32( 150, 90, 90, 255 ), 2.0f );
}

// Draws one explorer cell and returns true the frame it is clicked. The cell is the last ImGui
// item afterwards, so the caller can bind a context menu to it.
bool DrawExplorerItem(
    ExplorerContext& ioCtx, ExplorerItem inType, std::string const& inName, std::string const& inPath,
    bool inSelected ) noexcept
{
    float const labelHeight = ImGui::GetTextLineHeight();
    ImVec2 const cell( kCellWidth, kIconSize + labelHeight + 14.0f );
    ImVec2 const origin = ImGui::GetCursorScreenPos();

    bool const clicked = ImGui::Selectable( "##item", inSelected, 0, cell );
    bool const hovered = ImGui::IsItemHovered();

    ImDrawList* const list = ImGui::GetWindowDrawList();
    ImVec2 const iconMin( origin.x + ( kCellWidth - kIconSize ) * 0.5f, origin.y + 4.0f );
    switch ( inType )
    {
    case ExplorerItem::Folder:    DrawFolderIcon( list, iconMin ); break;
    case ExplorerItem::Animation: DrawAnimationIcon( list, iconMin ); break;
    case ExplorerItem::Audio:     DrawAudioIcon( list, iconMin ); break;
    case ExplorerItem::Font:      DrawFontIcon( list, iconMin ); break;
    case ExplorerItem::Texture:
        // Only a cell that is actually on screen loads its texture, so a folder of hundreds of
        // sprites does not read them all at once.
        if ( !ImGui::IsRectVisible( origin, ImVec2( origin.x + cell.x, origin.y + cell.y ) )
            || !DrawTextureThumbnail( list, iconMin, inPath, ioCtx ) )
        {
            DrawMissingTextureIcon( list, iconMin );
        }
        break;
    }

    std::string const label = FitText( inName, kCellWidth - 6.0f );
    float const labelWidth = ImGui::CalcTextSize( label.c_str() ).x;
    list->AddText(
        ImVec2( origin.x + ( kCellWidth - labelWidth ) * 0.5f, origin.y + kIconSize + 8.0f ),
        ImGui::GetColorU32( ImGuiCol_Text ), label.c_str() );

    if ( hovered ) ImGui::SetTooltip( "%s", inPath.c_str() );
    return clicked;
}

// The loaded and used path sets of one asset kind.
struct KindSets
{
    std::set<std::string>&       m_Loaded; // manually "Load Asset..."-ed: the only ones that can be removed
    std::set<std::string> const& m_Used;   // paths an entity references right now
};

KindSets SetsFor(
    AssetPickKind inKind, std::set<std::string> const& inUsedTextures, std::set<std::string> const& inUsedAnimations,
    std::set<std::string> const& inUsedAudio, std::set<std::string> const& inUsedFonts ) noexcept
{
    switch ( inKind )
    {
    case AssetPickKind::Animation: return { g_LoadedAnimations, inUsedAnimations };
    case AssetPickKind::Audio:     return { g_LoadedAudio, inUsedAudio };
    case AssetPickKind::Font:      return { g_LoadedFonts, inUsedFonts };
    default:                       return { g_LoadedTextures, inUsedTextures };
    }
}

ExplorerItem ItemTypeFor( AssetPickKind inKind ) noexcept
{
    switch ( inKind )
    {
    case AssetPickKind::Animation: return ExplorerItem::Animation;
    case AssetPickKind::Audio:     return ExplorerItem::Audio;
    case AssetPickKind::Font:      return ExplorerItem::Font;
    default:                       return ExplorerItem::Texture;
    }
}

// The path bar: an "up" button, then the mountpoint and every folder below it as a clickable
// crumb. Clicking a crumb opens that folder; the mountpoint's own crumb returns to its top.
void DrawBreadcrumbs( AssetExplorerState& ioState ) noexcept
{
    ImGui::BeginDisabled( ioState.m_Folder.empty() );
    if ( ImGui::ArrowButton( "##up", ImGuiDir_Up ) )
    {
        auto const slash = ioState.m_Folder.rfind( '/' );
        ioState.m_Folder = slash == std::string::npos ? std::string{} : ioState.m_Folder.substr( 0, slash );
    }
    ImGui::EndDisabled();
    if ( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) ) ImGui::SetTooltip( "Parent folder" );

    ImGui::SameLine();
    ImGui::PushID( "root" );
    if ( ImGui::SmallButton( ioState.m_Root.c_str() ) ) ioState.m_Folder.clear();
    ImGui::PopID();

    std::string target;
    std::string newFolder = ioState.m_Folder;
    std::size_t start = 0;
    while ( start < ioState.m_Folder.size() )
    {
        auto const slash = ioState.m_Folder.find( '/', start );
        std::string const segment = ioState.m_Folder.substr( start, slash == std::string::npos ? std::string::npos : slash - start );
        target += ( target.empty() ? "" : "/" ) + segment;

        ImGui::SameLine();
        ImGui::TextDisabled( "/" );
        ImGui::SameLine();
        ImGui::PushID( target.c_str() );
        if ( ImGui::SmallButton( segment.c_str() ) ) newFolder = target;
        ImGui::PopID();

        if ( slash == std::string::npos ) break;
        start = slash + 1;
    }
    ioState.m_Folder = newFolder;
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

std::vector<std::string> KnownFontPaths( asge::ecs::Registry& inRegistry ) noexcept
{
    auto const fonts = MergedFonts( inRegistry );
    return { fonts.begin(), fonts.end() };
}

void RegisterSceneAssets( asge::ecs::Registry& inRegistry ) noexcept
{
    std::set<std::string> textures, animations, audio, fonts;
    CollectUsedPaths( inRegistry, textures, animations, audio, fonts );
    g_LoadedTextures.insert( textures.begin(), textures.end() );
    g_LoadedAnimations.insert( animations.begin(), animations.end() );
    g_LoadedAudio.insert( audio.begin(), audio.end() );
    g_LoadedFonts.insert( fonts.begin(), fonts.end() );
}

void ImportAssets(
    std::vector<std::string> const& inTexturePaths,
    std::vector<std::string> const& inAnimationPaths,
    std::vector<std::string> const& inAudioPaths,
    std::vector<std::string> const& inFontPaths ) noexcept
{
    g_LoadedTextures.insert( inTexturePaths.begin(), inTexturePaths.end() );
    g_LoadedAnimations.insert( inAnimationPaths.begin(), inAnimationPaths.end() );
    g_LoadedAudio.insert( inAudioPaths.begin(), inAudioPaths.end() );
    g_LoadedFonts.insert( inFontPaths.begin(), inFontPaths.end() );
}

void ClearKnownAssets() noexcept
{
    g_LoadedTextures.clear();
    g_LoadedAnimations.clear();
    g_LoadedAudio.clear();
    g_LoadedFonts.clear();
    g_ThumbnailRetryAt.clear();
}

AssetBrowserResult DrawAssetExplorer(
    asge::ecs::Registry& inRegistry, asge::filesystem::VirtualFileSystem const& inVfs,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer, SDL_Window* inWindow,
    bool inHasProject, AssetExplorerState& ioState, AssetPick const& inSelected ) noexcept
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
                else if ( HasFontExtension( picked ) )
                {
                    g_LoadedFonts.insert( virtualPath );
                    LOG_INFO( "Loaded font ", virtualPath );
                }
                else
                {
                    LOG_WARNING(
                        "\"", virtualPath, "\" is not a recognized image, FrameTable clip, audio, or font file" );
                }
            }
        }
    }

    std::set<std::string> const textures = MergedTextures( inRegistry );
    std::set<std::string> const animations = MergedAnimations( inRegistry );
    std::set<std::string> const audio = MergedAudio( inRegistry );
    std::set<std::string> const fonts = MergedFonts( inRegistry );

    // Whether an entity's Sprite/Animation/AudioSource/UILabel actually
    // references a path right now -- checked before "Remove from list" is
    // allowed to un-import it. The already-loaded ITexture/clip/Font an entity
    // is using stays cached regardless of this panel's own bookkeeping, so
    // silently un-importing a path still in use wouldn't stop it rendering/
    // playing; it would just make the panel lie about what's actually bound.
    std::set<std::string> usedTextures, usedAnimations, usedAudio, usedFonts;
    CollectUsedPaths( inRegistry, usedTextures, usedAnimations, usedAudio, usedFonts );

    AssetBrowserResult result;

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
                    kAssetFilters, static_cast<int>( std::size( kAssetFilters ) ), defaultLocation.c_str(), false );
                ImGui::CloseCurrentPopup();
            }
        }
        if ( inVfs.ListMounts().empty() )
        {
            ImGui::TextDisabled( "(no mounts -- add one with + Add mount)" );
        }
        ImGui::EndPopup();
    }

    if ( ioState.m_Root.empty() )
    {
        ImGui::SameLine();
        ImGui::TextDisabled( "Select a mountpoint to browse its assets." );
        return result;
    }

    ImGui::SameLine();
    DrawBreadcrumbs( ioState );
    ImGui::Separator();

    // Every known asset path with its kind, so one folder listing covers all four kinds at once.
    std::map<std::string, AssetPickKind> kinds;
    std::set<std::string> allPaths;
    for ( auto const& path : textures )   { kinds[path] = AssetPickKind::Texture;   allPaths.insert( path ); }
    for ( auto const& path : animations ) { kinds[path] = AssetPickKind::Animation; allPaths.insert( path ); }
    for ( auto const& path : audio )      { kinds[path] = AssetPickKind::Audio;     allPaths.insert( path ); }
    for ( auto const& path : fonts )      { kinds[path] = AssetPickKind::Font;      allPaths.insert( path ); }

    std::string const folderPath = ioState.m_Folder.empty() ? ioState.m_Root : ioState.m_Root + "/" + ioState.m_Folder;
    FolderListing const listing = ListFolder( allPaths, folderPath );

    ExplorerContext ctx{ inAssets, inRenderer };

    ImGui::BeginChild( "##explorerGrid" );
    float const spacing = ImGui::GetStyle().ItemSpacing.x;
    int const columns = std::max( 1, static_cast<int>( ( ImGui::GetContentRegionAvail().x + spacing ) / ( kCellWidth + spacing ) ) );
    int placed = 0;
    auto const nextCell = [&]
    {
        if ( placed % columns != 0 ) ImGui::SameLine();
        ++placed;
    };

    // Folders first: a click enters the folder.
    std::string enterFolder;
    for ( auto const& name : listing.m_Folders )
    {
        nextCell();
        ImGui::PushID( ( "folder:" + name ).c_str() );
        if ( DrawExplorerItem( ctx, ExplorerItem::Folder, name, folderPath + "/" + name, false ) ) enterFolder = name;
        ImGui::PopID();
    }

    for ( auto const& name : listing.m_Files )
    {
        std::string const path = folderPath + "/" + name;
        AssetPickKind const kind = kinds[path];

        nextCell();
        ImGui::PushID( path.c_str() );
        bool const selected = inSelected.m_Kind == kind && inSelected.m_VirtualPath == path;
        if ( DrawExplorerItem( ctx, ItemTypeFor( kind ), name, path, selected ) )
        {
            result.m_Pick = { kind, path };
        }
        // Bound to the cell just drawn (BeginPopupContextItem with no explicit id ties to the last item).
        auto const sets = SetsFor( kind, usedTextures, usedAnimations, usedAudio, usedFonts );
        DrawAssetContextMenu(
            inRegistry, kind, path, kind == AssetPickKind::Texture, kind != AssetPickKind::Font,
            inHasProject, sets.m_Loaded, sets.m_Used, result );
        ImGui::PopID();
    }

    if ( listing.m_Folders.empty() && listing.m_Files.empty() )
    {
        ImGui::TextDisabled( "Nothing known in this folder -- use Load Asset... to bring a file in." );
    }
    ImGui::EndChild();

    if ( !enterFolder.empty() )
    {
        ioState.m_Folder = ioState.m_Folder.empty() ? enterFolder : ioState.m_Folder + "/" + enterFolder;
    }

    return result;
}
