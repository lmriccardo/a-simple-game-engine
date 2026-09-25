#include "AssetInspector.hpp"

#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>

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

/** @brief The audio preview's own playback state -- which clip's loaded and its stream, independent of any entity's AudioSource. */
struct AudioPreviewState
{
    std::string m_Path;
    std::shared_ptr<asge::audio::AudioStream> m_Stream;
};

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
    asge::audio::AudioDevice& inAudioDevice ) noexcept
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
        }
        else
        {
            ImGui::TextDisabled( "(preview unavailable)" );
        }
    }
    else if ( inSelectedAsset.m_Kind == AssetPickKind::Animation )
    {
        ImGui::TextDisabled( "No preview for animation clips." );
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

    ImGui::End();
    if ( !open ) ioSelectedAsset = AssetPick{};
}
