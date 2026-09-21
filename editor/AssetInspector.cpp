#include "AssetInspector.hpp"

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

/** @brief The part of inVirtualPath before its first '/' -- the mount it resolves against. */
std::string VirtualRootOf( std::string const& inVirtualPath ) noexcept
{
    auto const slash = inVirtualPath.find( '/' );
    return slash == std::string::npos ? inVirtualPath : inVirtualPath.substr( 0, slash );
}

// The preview texture for whichever path was last inspected -- reloaded
// only when that path changes, not every frame (CreateTexture allocates a
// fresh GPU texture on every call; calling it unconditionally every frame
// would leak one each time).
std::string           g_CachedPath;
asge::video::ITexture* g_CachedTexture = nullptr;

void RefreshPreviewIfNeeded(
    std::string const& inVirtualPath, asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer ) noexcept
{
    if ( g_CachedPath == inVirtualPath ) return;

    g_CachedPath = inVirtualPath;
    g_CachedTexture = nullptr;

    auto image = inAssets.GetImage( inVirtualPath );
    if ( !image )
    {
        image.LogError();
        return;
    }
    g_CachedTexture = inAssets.CreateTexture( inRenderer, image.Value()->Get() );
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
    ImGui::Text( "Mountpoint: %s", VirtualRootOf( inSelectedAsset.m_VirtualPath ).c_str() );

    std::error_code ec;
    auto const bytes = fs::file_size( resolved.Value(), ec );
    ImGui::Text( "File Size: %s", ec ? "unknown" : FormatSize( bytes ).c_str() );

    // Loaded here (not just below, next to the thumbnail) so Dimensions can
    // sit with the rest of the generic info, above the separator -- pixel
    // size is a fact about the asset, not part of the visual preview itself.
    if ( inSelectedAsset.m_Kind == AssetPickKind::Texture )
    {
        RefreshPreviewIfNeeded( inSelectedAsset.m_VirtualPath, inAssets, inRenderer );
        if ( g_CachedTexture )
        {
            auto const size = g_CachedTexture->Size();
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
        if ( g_CachedTexture )
        {
            auto const size = g_CachedTexture->Size();
            float const maxDim = 128.0f;
            float const largest = static_cast<float>( std::max( size.x(), size.y() ) );
            float const scale = largest > maxDim ? maxDim / largest : 1.0f;
            ImGui::Image(
                g_CachedTexture->NativeHandle(),
                ImVec2( static_cast<float>( size.x() ) * scale, static_cast<float>( size.y() ) * scale ) );
        }
        else
        {
            ImGui::TextDisabled( "(preview unavailable)" );
        }
    }
    else
    {
        ImGui::TextDisabled( "No preview for animation clips." );
    }

    ImGui::End();
    if ( !open ) ioSelectedAsset = AssetPick{};
}
