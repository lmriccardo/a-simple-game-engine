#include "AssetInspector.hpp"

#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
namespace fs = std::filesystem;

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
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer ) noexcept
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
    else
    {
        ImGui::TextDisabled( "No preview for audio clips." );
    }

    ImGui::End();
    if ( !open ) ioSelectedAsset = AssetPick{};
}
