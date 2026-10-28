#include "AssetInspector.hpp"
#include "Inspector.hpp"

#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/Formats/TOML/TOML_Parser.hpp>
#include <ASGE/Core/Formats/TOML/TOML_TableView.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Game/Assets/FrameTable.hpp>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <SDL3/SDL_audio.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
namespace fs = std::filesystem;

/** @brief Draft grid for the "Create Clip" modal -- reset whenever it's (re)opened, from whichever texture the button was clicked on. */
struct CreateClipDraft
{
    int   m_Rows{1};
    int   m_Columns{1};
    int   m_First{0};
    int   m_Count{1};
    float m_MarginX{0.0f};
    float m_MarginY{0.0f};
    float m_Spacing{0.0f};
    int   m_Order{0}; // 0 = row-major, 1 = column-major (GridOrder)
    std::string m_Name{};
};
CreateClipDraft g_CreateClip;

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
// has, not any particular entity's playback speed. Used for any frame the
// clip gives no duration of its own.
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
    asge::audio::AudioDevice& inAudioDevice, asge::ecs::Registry& inRegistry,
    bool inOpenCreateClip ) noexcept
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
    else if ( inSelectedAsset.m_Kind == AssetPickKind::Font )
    {
        // FindMaxPixelHeight does several bake attempts (binary search) --
        // cached by path, not recomputed every single frame this panel
        // happens to be open.
        static std::string s_CachedPath;
        static std::optional<int> s_CachedMax;
        if ( s_CachedPath != inSelectedAsset.m_VirtualPath )
        {
            s_CachedPath = inSelectedAsset.m_VirtualPath;
            auto maxResult = asge::media::Font::FindMaxPixelHeight( resolved.Value() );
            s_CachedMax = maxResult ? std::optional<int>( maxResult.Value() ) : std::nullopt;
            if ( !maxResult ) maxResult.LogError();
        }
        if ( s_CachedMax ) ImGui::Text( "Max Size: %d px", *s_CachedMax );
        else ImGui::Text( "Max Size: unknown" );
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
            std::vector<std::string> associatedClips;
            for ( auto const& animPath : KnownAnimationPaths( inRegistry ) )
            {
                auto ft = inAssets.GetFrameTable( animPath );
                if ( ft && std::string( ft.Value()->Get().m_OriginAsset ) == inSelectedAsset.m_VirtualPath )
                {
                    associatedClips.push_back( animPath );
                }
            }

            // A texture can carry any number of named clips, so one button each.
            for ( auto const& clip : associatedClips )
            {
                ImGui::PushID( clip.c_str() );
                if ( ImGui::Button( ( "Open Clip: " + ClipNameFromPath( clip ) ).c_str() ) )
                {
                    ioSelectedAsset = AssetPick{ AssetPickKind::Animation, clip };
                }
                ImGui::PopID();
            }

            // Phase 14: "Create Clip" itself moved out to the Assets panel's
            // row context menu -- this just reacts to that request, one
            // frame after it's set (ioSelectedAsset already names this
            // texture by the time main.cpp gets here, same frame as the
            // click). A texture can have any number of clips, so this always
            // opens the modal.
            if ( inOpenCreateClip )
            {
                g_CreateClip = CreateClipDraft{};
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
            int rows = table.Get<int>( "rows", 0 );
            if ( rows == 0 && columns > 0 ) rows = ( count + columns - 1 ) / columns; // older clips don't store rows -- fall back to ceil(count / columns)
            ImGui::Text( "columns: %d", columns );
            ImGui::Text( "rows: %d", rows );
            ImGui::Text( "first: %d", table.Get<int>( "first", 0 ) );
            ImGui::Text( "count: %d", count );
            ImGui::Text( "spacing: %.1f", table.Get( "spacing", 0.0f ) );
            ImGui::Text( "order: %s", table.Get( "order", std::string( "row" ) ).c_str() );

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
                    if ( preview.m_CurrentFrame >= frames.size() ) preview.m_CurrentFrame = 0;
                    auto const& previewTable = frameTableResult.Value()->Get();
                    while ( preview.m_ElapsedTime >= previewTable.FrameDuration( preview.m_CurrentFrame, kAnimationPreviewFrameDuration ) )
                    {
                        preview.m_ElapsedTime -= previewTable.FrameDuration( preview.m_CurrentFrame, kAnimationPreviewFrameDuration );
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
    else if ( inSelectedAsset.m_Kind == AssetPickKind::Font )
    {
        // A fixed preview size, independent of whatever pixel height any
        // UILabel using this font happens to be resolved at -- this is
        // previewing the font file itself, not any particular entity's own
        // setting (same reasoning as the animation clip preview's own fixed
        // frame duration).
        constexpr int kPreviewPixelHeight = 28;
        auto fontResult = inAssets.GetFont( inSelectedAsset.m_VirtualPath, kPreviewPixelHeight );
        if ( !fontResult )
        {
            fontResult.LogError();
            ImGui::TextDisabled( "(preview unavailable)" );
        }
        else
        {
            auto& font = fontResult.Value()->Get();
            auto texResult = inAssets.GetFontAtlasTexture( font, inRenderer );
            if ( !texResult )
            {
                ImGui::TextDisabled( "(preview unavailable)" );
            }
            else
            {
                auto* atlasTexture = texResult.Value();
                auto const atlasSize = atlasTexture->Size();
                constexpr std::string_view kSampleText = "AaBbCc 0123";

                // Same per-glyph layout SDLRenderer::DrawString uses (pen
                // advances by each glyph's own advance, positioned by its
                // bearing) -- drawn via ImGui::Image-style UVs instead of an
                // IRenderer::DrawXxx call, since this panel has no scene
                // camera/viewport of its own to draw into (same reason the
                // texture/animation previews above use ImGui::Image rather
                // than IRenderer::DrawTexture).
                ImVec2 const origin = ImGui::GetCursorScreenPos();
                auto* drawList = ImGui::GetWindowDrawList();
                float penX = origin.x;
                float const penY = origin.y + static_cast<float>( font.GetAscent() );

                for ( char c : kSampleText )
                {
                    auto glyphResult = font.GetGlyph( static_cast<char32_t>( c ) );
                    if ( !glyphResult ) continue;

                    auto const& glyph = glyphResult.Value();
                    ImVec2 const p0{
                        penX + static_cast<float>( glyph.bearing.x() ), penY + static_cast<float>( glyph.bearing.y() ) };
                    ImVec2 const p1{ p0.x + static_cast<float>( glyph.size.x() ), p0.y + static_cast<float>( glyph.size.y() ) };
                    ImVec2 const uv0{
                        glyph.uv_rect.m_X / static_cast<float>( atlasSize.x() ),
                        glyph.uv_rect.m_Y / static_cast<float>( atlasSize.y() ) };
                    ImVec2 const uv1{
                        ( glyph.uv_rect.m_X + glyph.uv_rect.m_Width ) / static_cast<float>( atlasSize.x() ),
                        ( glyph.uv_rect.m_Y + glyph.uv_rect.m_Height ) / static_cast<float>( atlasSize.y() ) };

                    drawList->AddImage( atlasTexture->NativeHandle(), p0, p1, uv0, uv1 );
                    penX += static_cast<float>( glyph.advance );
                }

                // Reserves the actual layout space the drawn text occupies,
                // so the panel's own scrolling/sizing accounts for it --
                // AddImage above draws straight to the window's draw list,
                // which doesn't advance ImGui's own cursor on its own.
                ImGui::Dummy( ImVec2( penX - origin.x, static_cast<float>( font.GetLineHeight() ) ) );
            }
        }
    }

    if ( ImGui::BeginPopupModal( "Create Clip", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
    {
        if ( texture )
        {
            auto const size = texture->Size();
            // Always a preview, never 1:1: scaled (up or down) so the sprite's
            // largest side fills ~60% of the shorter viewport side, capped so
            // the modal stays usable on small windows.
            auto const viewportSize = ImGui::GetMainViewport()->Size;
            float const maxDim = std::clamp( 0.6f * std::min( viewportSize.x, viewportSize.y ), 256.0f, 720.0f );
            float const largest = static_cast<float>( std::max( size.x(), size.y() ) );
            float const scale = maxDim / largest;
            ImVec2 const imageSize( static_cast<float>( size.x() ) * scale, static_cast<float>( size.y() ) * scale );

            ImVec2 const imagePos = ImGui::GetCursorScreenPos();
            ImGui::Image( texture->NativeHandle(), imageSize );

            auto& d = g_CreateClip;
            ImGui::InputTextWithHint( "Name", "e.g. walk", &d.m_Name );
            auto const texW = static_cast<float>( size.x() );
            auto const texH = static_cast<float>( size.y() );

            ImGui::Separator();

            if ( ImGui::InputInt( "Rows", &d.m_Rows ) ) d.m_Rows = std::max( 1, d.m_Rows );
            if ( ImGui::InputInt( "Columns", &d.m_Columns ) ) d.m_Columns = std::max( 1, d.m_Columns );
            ImGui::InputFloat( "Margin X", &d.m_MarginX, 1.0f, 8.0f, "%.1f" );
            ImGui::InputFloat( "Margin Y", &d.m_MarginY, 1.0f, 8.0f, "%.1f" );
            ImGui::InputFloat( "Spacing", &d.m_Spacing, 1.0f, 8.0f, "%.1f" );
            d.m_MarginX = std::max( 0.0f, d.m_MarginX );
            d.m_MarginY = std::max( 0.0f, d.m_MarginY );
            d.m_Spacing = std::max( 0.0f, d.m_Spacing );
            ImGui::RadioButton( "Row-major", &d.m_Order, 0 );
            ImGui::SameLine();
            ImGui::RadioButton( "Column-major", &d.m_Order, 1 );

            int const gridCells = d.m_Rows * d.m_Columns;
            if ( ImGui::InputInt( "First frame", &d.m_First ) ) d.m_First = std::clamp( d.m_First, 0, gridCells - 1 );
            if ( ImGui::InputInt( "Frame count", &d.m_Count ) ) d.m_Count = std::max( 1, d.m_Count );
            d.m_First = std::clamp( d.m_First, 0, gridCells - 1 );
            d.m_Count = std::clamp( d.m_Count, 1, gridCells - d.m_First );

            // Margin is applied on every side of the sheet, so the cell size
            // is whatever is left once margins and gaps are taken out.
            float const cellW = ( texW - 2.0f * d.m_MarginX - d.m_Spacing * static_cast<float>( d.m_Columns - 1 ) )
                                / static_cast<float>( d.m_Columns );
            float const cellH = ( texH - 2.0f * d.m_MarginY - d.m_Spacing * static_cast<float>( d.m_Rows - 1 ) )
                                / static_cast<float>( d.m_Rows );
            // The name ends up in a file name, so keep it to a safe charset.
            bool const nameValid = !d.m_Name.empty() && std::all_of( d.m_Name.begin(), d.m_Name.end(), []( unsigned char c ) {
                return std::isalnum( c ) || c == '_' || c == '-';
            } );
            bool const valid = cellW > 0.0f && cellH > 0.0f;
            ImGui::Text( "Cell size: %.1f x %.1f px", cellW, cellH );
            if ( !valid ) ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "Margins/spacing leave no room for cells" );

            asge::game::asset::GridSpec const spec{
                .m_Cell    = { d.m_MarginX, d.m_MarginY, cellW, cellH },
                .m_Columns = static_cast<std::size_t>( d.m_Columns ),
                .m_Rows    = static_cast<std::size_t>( d.m_Rows ),
                .m_First   = static_cast<std::size_t>( d.m_First ),
                .m_Count   = static_cast<std::size_t>( d.m_Count ),
                .m_Spacing = d.m_Spacing,
                .m_Order   = d.m_Order == 1 ? asge::game::asset::GridOrder::ColumnMajor
                                            : asge::game::asset::GridOrder::RowMajor,
            };

            // Live preview over the thumbnail: every cell of the grid faintly
            // (so the whole layout being defined is visible), then the exact
            // rects the clip will contain -- same MakeGridFrames the saved
            // file is loaded through -- highlighted and numbered in playback order.
            if ( valid )
            {
                auto* drawList = ImGui::GetWindowDrawList();
                constexpr ImU32 kGridColor = IM_COL32( 255, 255, 255, 70 );
                constexpr ImU32 kClipColor = IM_COL32( 255, 220, 0, 255 );
                float const sx = imageSize.x / texW;
                float const sy = imageSize.y / texH;
                auto const toScreen = [&]( asge::math::Rect const& f ) {
                    ImVec2 const p0( imagePos.x + f.m_X * sx, imagePos.y + f.m_Y * sy );
                    return std::pair( p0, ImVec2( p0.x + f.m_Width * sx, p0.y + f.m_Height * sy ) );
                };

                auto gridSpec = spec;
                gridSpec.m_First = 0;
                gridSpec.m_Count = static_cast<std::size_t>( gridCells );
                for ( auto const& f : asge::game::asset::MakeGridFrames( gridSpec ) )
                {
                    auto const [p0, p1] = toScreen( f );
                    drawList->AddRect( p0, p1, kGridColor );
                }

                int idx = 0;
                for ( auto const& f : asge::game::asset::MakeGridFrames( spec ) )
                {
                    auto const [p0, p1] = toScreen( f );
                    drawList->AddRect( p0, p1, kClipColor, 0.0f, 0, 2.0f );
                    char label[16];
                    std::snprintf( label, sizeof label, "%d", idx++ );
                    drawList->AddText( ImVec2( p0.x + 2.0f, p0.y + 1.0f ), kClipColor, label );
                }
            }

            if ( ImGui::Button( "Cancel" ) ) ImGui::CloseCurrentPopup();
            ImGui::SameLine();
            std::string const clipSuffix = "_clip_" + d.m_Name + ".toml";
            bool const nameTaken = nameValid && fs::exists( resolved.Value().parent_path() / ( resolved.Value().stem().string() + clipSuffix ) );
            if ( !d.m_Name.empty() && !nameValid ) ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "Name: letters, digits, '_' and '-' only" );
            if ( nameTaken ) ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "A clip with this name already exists" );

            bool const canApply = valid && nameValid && !nameTaken;
            if ( !canApply ) ImGui::BeginDisabled();
            bool const apply = ImGui::Button( "Apply" );
            if ( !canApply ) ImGui::EndDisabled();
            if ( apply )
            {
                // Clip file sits next to the texture, same stem plus
                // "_clip_<name>.toml" -- both as a real path (to write to) and as
                // a virtual path (to register/select), derived the same way.
                fs::path const clipRealPath =
                    resolved.Value().parent_path() / ( resolved.Value().stem().string() + clipSuffix );
                fs::path const texVirtualPath( inSelectedAsset.m_VirtualPath );
                std::string const clipVirtualPath =
                    ( texVirtualPath.parent_path() / ( texVirtualPath.stem().string() + clipSuffix ) ).generic_string();
                std::string const originVirtualPath = inSelectedAsset.m_VirtualPath;

                auto const saveResult = asge::game::asset::FrameTable::Save( clipRealPath, spec, originVirtualPath );

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
