#include "AssetInspector.hpp"

#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Configuration/TOML_Parser.hpp>
#include <ASGE/Core/Configuration/TOML_TableView.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Game/Assets/FrameTable.hpp>

#include <imgui.h>

#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
namespace fs = std::filesystem;

// Draft Rows/Columns for the "Create Clip" modal -- reset whenever it's
// (re)opened, from whichever texture the button was clicked on.
int g_CreateClipRows = 1;
int g_CreateClipColumns = 1;

/** @brief The audio preview's own playback state -- which clip's loaded and its stream, independent of any entity's AudioSource. */
struct AudioPreviewState
{
    std::string m_Path;
    std::shared_ptr<asge::audio::AudioStream> m_Stream;
};

/** @brief The animation clip preview's own cycling state -- which clip's showing and where in its frame loop, independent of any entity's Animation. */
struct AnimationPreviewState
{
    std::string m_Path;
    std::size_t m_CurrentFrame = 0;
    float       m_ElapsedTime = 0.0f;
};

// Fixed cadence for the clip preview, independent of any entity's own
// Animation::m_FrameDuration -- this is previewing what frames the clip
// has, not any particular entity's playback speed.
constexpr float kAnimationPreviewFrameDuration = 0.15f;

/** @brief A small transport-control button drawn as a play triangle or pause bars (no icon font in this project) -- returns true the frame it's clicked. */
bool PlayPauseIconButton( char const* inId, bool inPlaying ) noexcept
{
    ImVec2 const size( 20.0f, 20.0f );
    ImGui::InvisibleButton( inId, size );
    bool const clicked = ImGui::IsItemClicked();
    ImU32 const color = ImGui::GetColorU32(
        ImGui::IsItemHovered() ? ImGuiCol_ButtonHovered : ImGuiCol_Text );

    auto* drawList = ImGui::GetWindowDrawList();
    ImVec2 const p0 = ImGui::GetItemRectMin();
    ImVec2 const p1 = ImGui::GetItemRectMax();
    ImVec2 const center( ( p0.x + p1.x ) * 0.5f, ( p0.y + p1.y ) * 0.5f );
    float const r = 6.0f;

    if ( inPlaying )
    {
        float const barW = 4.0f;
        float const gap = 3.0f;
        drawList->AddRectFilled(
            ImVec2( center.x - gap - barW, center.y - r ), ImVec2( center.x - gap, center.y + r ), color );
        drawList->AddRectFilled(
            ImVec2( center.x + gap, center.y - r ), ImVec2( center.x + gap + barW, center.y + r ), color );
    }
    else
    {
        drawList->AddTriangleFilled(
            ImVec2( center.x - r * 0.6f, center.y - r ),
            ImVec2( center.x - r * 0.6f, center.y + r ),
            ImVec2( center.x + r * 0.8f, center.y ),
            color );
    }

    return clicked;
}

/** @brief A rewind-to-start transport-control button (bar + left-pointing triangle) -- returns true the frame it's clicked. */
bool RewindIconButton( char const* inId ) noexcept
{
    ImVec2 const size( 20.0f, 20.0f );
    ImGui::InvisibleButton( inId, size );
    bool const clicked = ImGui::IsItemClicked();
    ImU32 const color = ImGui::GetColorU32(
        ImGui::IsItemHovered() ? ImGuiCol_ButtonHovered : ImGuiCol_Text );

    auto* drawList = ImGui::GetWindowDrawList();
    ImVec2 const p0 = ImGui::GetItemRectMin();
    ImVec2 const p1 = ImGui::GetItemRectMax();
    ImVec2 const center( ( p0.x + p1.x ) * 0.5f, ( p0.y + p1.y ) * 0.5f );
    float const r = 6.0f;

    drawList->AddRectFilled(
        ImVec2( center.x - r * 1.1f, center.y - r ), ImVec2( center.x - r * 0.6f, center.y + r ), color );
    drawList->AddTriangleFilled(
        ImVec2( center.x + r * 0.8f, center.y - r ),
        ImVec2( center.x + r * 0.8f, center.y + r ),
        ImVec2( center.x - r * 0.4f, center.y ),
        color );

    return clicked;
}

std::string FormatSize( std::uintmax_t inBytes ) noexcept
{
    constexpr double kKB = 1024.0;
    constexpr double kMB = kKB * 1024.0;

    std::ostringstream oss;
    if ( inBytes >= static_cast<std::uintmax_t>( kMB ) )
    {
        oss << std::fixed << std::setprecision( 2 ) << ( static_cast<double>( inBytes ) / kMB ) << " MB";
    }
    else if ( inBytes >= static_cast<std::uintmax_t>( kKB ) )
    {
        oss << std::fixed << std::setprecision( 1 ) << ( static_cast<double>( inBytes ) / kKB ) << " KB";
    }
    else
    {
        oss << inBytes << " B";
    }
    return oss.str();
}
}

void DrawAssetInspectorPanel(
    AssetPick& ioSelectedAsset, asge::filesystem::VirtualFileSystem const& inVfs,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    asge::audio::AudioDevice& inAudioDevice, asge::ecs::Registry& inRegistry ) noexcept
{
    if ( ioSelectedAsset.m_Kind == AssetPickKind::None ) return;
    AssetPick const& inSelectedAsset = ioSelectedAsset; // read-only from here down

    bool open = true;
    ImGui::SetNextWindowPos( ImVec2( 300.0f, 30.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 260.0f, 260.0f ), ImGuiCond_FirstUseEver );
    ImGui::Begin( "Asset Inspector", &open );

    auto const resolved = inVfs.Resolve( inSelectedAsset.m_VirtualPath );
    if ( !resolved )
    {
        ImGui::TextColored(
            ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "Could not resolve \"%s\"", inSelectedAsset.m_VirtualPath.c_str() );
        ImGui::End();
        if ( !open ) ioSelectedAsset = AssetPick{};
        return;
    }

    std::string const absPath = resolved.Value().string();
    ImGui::TextWrapped( "Absolute Path: %s", absPath.c_str() );
    ImGui::Text( "Mountpoint: %s",
        asge::filesystem::VirtualFileSystem::SplitRoot( inSelectedAsset.m_VirtualPath ).m_Root.c_str() );

    std::error_code ec;
    auto const bytes = fs::file_size( resolved.Value(), ec );
    ImGui::Text( "File Size: %s", ec ? "unknown" : FormatSize( bytes ).c_str() );

    // GetTexture is path-cached by AssetManager itself, so calling it every
    // frame just returns the same ITexture* rather than reloading anything.
    asge::video::ITexture* texture = nullptr;
    if ( inSelectedAsset.m_Kind == AssetPickKind::Texture )
    {
        if ( auto r = inAssets.GetTexture( inSelectedAsset.m_VirtualPath, inRenderer ) ) texture = r.Value();
        else r.LogError();
    }

    // Loaded here (not just below, next to the thumbnail) so Dimensions can
    // sit with the rest of the generic info, above the separator -- pixel
    // size is a fact about the asset, not part of the visual preview itself.
    if ( inSelectedAsset.m_Kind == AssetPickKind::Texture )
    {
        if ( texture )
        {
            auto const size = texture->Size();
            ImGui::Text( "Dimensions: %d x %d px", size.x(), size.y() );
        }
        else
        {
            ImGui::Text( "Dimensions: unknown" );
        }
    }

    ImGui::Separator();

    if ( inSelectedAsset.m_Kind == AssetPickKind::Texture )
    {
        if ( texture )
        {
            auto const size = texture->Size();
            float const maxDim = 128.0f;
            float const largest = static_cast<float>( std::max( size.x(), size.y() ) );
            float const scale = largest > maxDim ? maxDim / largest : 1.0f;
            ImGui::Image(
                texture->NativeHandle(),
                ImVec2( static_cast<float>( size.x() ) * scale, static_cast<float>( size.y() ) * scale ) );

            // Recomputed fresh every frame -- see the header doc comment
            // for why this alone also handles reverting to "Create Clip"
            // once an associated clip is unloaded from the Assets panel.
            std::string associatedClip;
            for ( auto const& animPath : KnownAnimationPaths( inRegistry ) )
            {
                auto ft = inAssets.GetFrameTable( animPath );
                if ( ft && std::string( ft.Value()->Get().m_OriginAsset ) == inSelectedAsset.m_VirtualPath )
                {
                    associatedClip = animPath;
                    break;
                }
            }

            if ( !associatedClip.empty() )
            {
                if ( ImGui::Button( "Open Clip" ) )
                {
                    ioSelectedAsset = AssetPick{ AssetPickKind::Animation, associatedClip };
                }
            }
            else if ( ImGui::Button( "Create Clip" ) )
            {
                g_CreateClipRows = 1;
                g_CreateClipColumns = 1;
                ImGui::OpenPopup( "Create Clip" );
            }
        }
        else
        {
            ImGui::TextDisabled( "(preview unavailable)" );
        }
    }
    else if ( inSelectedAsset.m_Kind == AssetPickKind::Animation )
    {
        // Read straight from the file rather than through
        // AssetManager::GetFrameTable -- that returns the already-expanded
        // per-frame rect list, not the raw x/y/w/h/columns/count fields the
        // requirement (and the "Create Clip" modal below) actually author.
        auto parsedClip = asge::config::toml::Parse( resolved.Value() );
        if ( !parsedClip )
        {
            ImGui::TextDisabled( "(preview unavailable)" );
        }
        else
        {
            asge::config::toml::TOMLTableView root( parsedClip.Value() );
            auto table = root.Table( "FrameTable" );
            std::string const origin = table.Get( "m_OriginAsset", std::string{} );
            ImGui::Text( "Origin Asset: %s", origin.empty() ? "(none)" : origin.c_str() );
            ImGui::Text( "x: %.1f", table.Get( "x", 0.0f ) );
            ImGui::Text( "y: %.1f", table.Get( "y", 0.0f ) );
            ImGui::Text( "w: %.1f", table.Get( "w", 0.0f ) );
            ImGui::Text( "h: %.1f", table.Get( "h", 0.0f ) );
            int const columns = table.Get<int>( "columns", 0 );
            int const count = table.Get<int>( "count", 0 );
            int const rows = columns > 0 ? ( count + columns - 1 ) / columns : 0; // ceil(count / columns) -- not its own stored field, just derived for display
            ImGui::Text( "columns: %d", columns );
            ImGui::Text( "rows: %d", rows );
            ImGui::Text( "count: %d", count );

            ImGui::Separator();

            static AnimationPreviewState preview;
            if ( preview.m_Path != inSelectedAsset.m_VirtualPath )
            {
                preview = AnimationPreviewState{};
                preview.m_Path = inSelectedAsset.m_VirtualPath;
            }

            // GetFrameTable (not the raw parse above) for the actual
            // per-frame rects to crop -- cached by AssetManager same as
            // GetTexture/GetAudio, so this is cheap after the first call.
            auto frameTableResult = inAssets.GetFrameTable( inSelectedAsset.m_VirtualPath );
            if ( !frameTableResult || frameTableResult.Value()->Get().m_Frames.empty() )
            {
                ImGui::TextDisabled( "(no frames to preview)" );
            }
            else if ( origin.empty() )
            {
                ImGui::TextDisabled( "(no Origin Asset set -- cannot preview)" );
            }
            else
            {
                auto texResult = inAssets.GetTexture( origin, inRenderer );
                if ( !texResult )
                {
                    ImGui::TextDisabled( "(origin texture unavailable)" );
                }
                else
                {
                    auto* originTexture = texResult.Value();
                    auto const& frames = frameTableResult.Value()->Get().m_Frames;

                    preview.m_ElapsedTime += ImGui::GetIO().DeltaTime;
                    while ( preview.m_ElapsedTime >= kAnimationPreviewFrameDuration )
                    {
                        preview.m_ElapsedTime -= kAnimationPreviewFrameDuration;
                        preview.m_CurrentFrame = ( preview.m_CurrentFrame + 1 ) % frames.size();
                    }

                    auto const& frame = frames[preview.m_CurrentFrame];
                    auto const texSize = originTexture->Size();
                    ImVec2 const uv0( frame.m_X / static_cast<float>( texSize.x() ), frame.m_Y / static_cast<float>( texSize.y() ) );
                    ImVec2 const uv1(
                        ( frame.m_X + frame.m_Width ) / static_cast<float>( texSize.x() ),
                        ( frame.m_Y + frame.m_Height ) / static_cast<float>( texSize.y() ) );

                    float const maxDim = 128.0f;
                    float const largest = std::max( frame.m_Width, frame.m_Height );
                    float const scale = largest > maxDim ? maxDim / largest : 1.0f;
                    ImGui::Image(
                        originTexture->NativeHandle(),
                        ImVec2( frame.m_Width * scale, frame.m_Height * scale ), uv0, uv1 );
                }
            }
        }
    }
    else if ( inSelectedAsset.m_Kind == AssetPickKind::Audio )
    {
        static AudioPreviewState preview;

        auto clipResult = inAssets.GetAudio( inSelectedAsset.m_VirtualPath );
        if ( !clipResult )
        {
            clipResult.LogError();
            ImGui::TextDisabled( "(preview unavailable)" );
        }
        else
        {
            auto& clip = clipResult.Value()->Get();

            // A different clip than last frame -- drop the old preview
            // stream so switching selections doesn't keep the previous one
            // queued/playing underneath the newly-shown one.
            if ( preview.m_Path != inSelectedAsset.m_VirtualPath )
            {
                if ( preview.m_Stream )
                {
                    if ( auto r = inAudioDevice.DetachStream( *preview.m_Stream ); !r ) r.LogError();
                }
                preview.m_Stream.reset();
                preview.m_Path = inSelectedAsset.m_VirtualPath;
            }

            auto const frameSize = SDL_AUDIO_FRAMESIZE( clip.Spec() );
            double const seconds = ( frameSize > 0 && clip.Spec().freq > 0 )
                ? static_cast<double>( clip.Size() ) / static_cast<double>( frameSize ) / clip.Spec().freq
                : 0.0;
            ImGui::Text( "Duration: %d:%02d", static_cast<int>( seconds ) / 60, static_cast<int>( seconds ) % 60 );
            ImGui::Text( "Size: %.1f KB", static_cast<double>( clip.Size() ) / 1024.0 );

            bool const hasLiveStream = preview.m_Stream && preview.m_Stream->IsValid();
            bool const playing = hasLiveStream && preview.m_Stream->IsDataAvailable()
                && !SDL_AudioStreamDevicePaused( preview.m_Stream->Get() );

            if ( PlayPauseIconButton( "##AudioPreviewPlayPause", playing ) )
            {
                if ( !hasLiveStream )
                {
                    auto streamResult = inAudioDevice.CreateStream( clip );
                    if ( !streamResult ) streamResult.LogError();
                    else
                    {
                        preview.m_Stream = streamResult.Value();
                        preview.m_Stream->PutData( clip );
                    }
                }
                else if ( !preview.m_Stream->IsDataAvailable() )
                {
                    // Ran out on its own -- Play means "again", same as Rewind.
                    preview.m_Stream->ClearData();
                    preview.m_Stream->PutData( clip );
                    SDL_ResumeAudioStreamDevice( preview.m_Stream->Get() );
                }
                else if ( playing )
                {
                    SDL_PauseAudioStreamDevice( preview.m_Stream->Get() );
                }
                else
                {
                    SDL_ResumeAudioStreamDevice( preview.m_Stream->Get() );
                }
            }

            ImGui::SameLine();
            if ( RewindIconButton( "##AudioPreviewRewind" ) && hasLiveStream )
            {
                preview.m_Stream->ClearData();
                preview.m_Stream->PutData( clip );
            }
        }
    }

    if ( ImGui::BeginPopupModal( "Create Clip", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        if ( texture )
        {
            auto const size = texture->Size();
            float const maxDim = 256.0f;
            float const largest = static_cast<float>( std::max( size.x(), size.y() ) );
            float const scale = largest > maxDim ? maxDim / largest : 1.0f;
            ImVec2 const imageSize( static_cast<float>( size.x() ) * scale, static_cast<float>( size.y() ) * scale );

            ImVec2 const imagePos = ImGui::GetCursorScreenPos();
            ImGui::Image( texture->NativeHandle(), imageSize );

            // Live grid preview over the thumbnail, redrawn every frame
            // from whatever Rows/Columns is currently set below.
            auto* drawList = ImGui::GetWindowDrawList();
            constexpr ImU32 kGridColor = IM_COL32( 255, 255, 255, 200 );
            for ( int c = 1; c < g_CreateClipColumns; ++c )
            {
                float const x = imagePos.x + imageSize.x * ( static_cast<float>( c ) / static_cast<float>( g_CreateClipColumns ) );
                drawList->AddLine( ImVec2( x, imagePos.y ), ImVec2( x, imagePos.y + imageSize.y ), kGridColor );
            }
            for ( int r = 1; r < g_CreateClipRows; ++r )
            {
                float const y = imagePos.y + imageSize.y * ( static_cast<float>( r ) / static_cast<float>( g_CreateClipRows ) );
                drawList->AddLine( ImVec2( imagePos.x, y ), ImVec2( imagePos.x + imageSize.x, y ), kGridColor );
            }

            ImGui::Separator();

            if ( ImGui::InputInt( "Rows", &g_CreateClipRows ) ) g_CreateClipRows = std::max( 1, g_CreateClipRows );
            if ( ImGui::InputInt( "Columns", &g_CreateClipColumns ) ) g_CreateClipColumns = std::max( 1, g_CreateClipColumns );

            float const cellW = static_cast<float>( size.x() ) / static_cast<float>( g_CreateClipColumns );
            float const cellH = static_cast<float>( size.y() ) / static_cast<float>( g_CreateClipRows );
            ImGui::Text( "Cell size: %.1f x %.1f px", cellW, cellH );

            if ( ImGui::Button( "Cancel" ) ) ImGui::CloseCurrentPopup();
            ImGui::SameLine();
            if ( ImGui::Button( "Apply" ) )
            {
                // Clip file sits next to the texture, same stem plus
                // "_clip.toml" -- both as a real path (to write to) and as
                // a virtual path (to register/select), derived the same way.
                fs::path const clipRealPath =
                    resolved.Value().parent_path() / ( resolved.Value().stem().string() + "_clip.toml" );
                fs::path const texVirtualPath( inSelectedAsset.m_VirtualPath );
                std::string const clipVirtualPath =
                    ( texVirtualPath.parent_path() / ( texVirtualPath.stem().string() + "_clip.toml" ) ).generic_string();
                std::string const originVirtualPath = inSelectedAsset.m_VirtualPath;

                auto const saveResult = asge::game::asset::FrameTable::Save(
                    clipRealPath, asge::math::Rect{ 0.0f, 0.0f, cellW, cellH },
                    static_cast<std::size_t>( g_CreateClipColumns ),
                    static_cast<std::size_t>( g_CreateClipRows * g_CreateClipColumns ),
                    originVirtualPath );

                if ( !saveResult )
                {
                    saveResult.LogError();
                }
                else
                {
                    ImportAssets( { originVirtualPath }, { clipVirtualPath }, {} );
                    ioSelectedAsset = AssetPick{ AssetPickKind::Animation, clipVirtualPath };
                    LOG_INFO( "Animation clip created at ", clipRealPath.string() );
                }
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            ImGui::TextDisabled( "(texture unavailable)" );
            if ( ImGui::Button( "Close" ) ) ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
    if ( !open ) ioSelectedAsset = AssetPick{};
}
