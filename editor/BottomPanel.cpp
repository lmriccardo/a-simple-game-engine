#include "BottomPanel.hpp"
#include "ConsolePanel.hpp"
#include "VfsPanel.hpp"

#include <imgui.h>

#include <algorithm>

namespace
{
constexpr float kSplitterWidth = 6.0f;
constexpr float kMinColumnWidth = 120.0f;
constexpr float kMinHeight = 80.0f;
constexpr float kReservedAbove = 60.0f; // menu bar and a little room above the panel, never covered by it

bool g_Minimized = false;
float g_Height = 240.0f;       // last user-dragged height, kept so the forced anchor below doesn't undo a drag
float g_MountsWidth = 280.0f;  // VFS column
float g_AssetsWidth = 380.0f;  // asset browser column; the Log gets the rest
AssetExplorerState g_Explorer; // selected mountpoint and open folder, shared by the mounts and asset columns

// A thin vertical grip between two columns: dragging it changes ioWidth
// (the column to its left) within [kMinColumnWidth, inMaxWidth].
void Splitter( char const* inId, float& ioWidth, float inHeight, float inMaxWidth ) noexcept
{
    ImGui::SameLine( 0.0f, 0.0f );
    ImGui::InvisibleButton( inId, ImVec2( kSplitterWidth, inHeight ) );
    bool const active = ImGui::IsItemActive();
    if ( ImGui::IsItemHovered() || active ) ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeEW );
    if ( active ) ioWidth += ImGui::GetIO().MouseDelta.x;
    ioWidth = std::clamp( ioWidth, kMinColumnWidth, std::max( kMinColumnWidth, inMaxWidth ) );

    ImVec2 const lo = ImGui::GetItemRectMin();
    ImVec2 const hi = ImGui::GetItemRectMax();
    float const x = ( lo.x + hi.x ) * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2( x, lo.y ), ImVec2( x, hi.y ),
        ImGui::GetColorU32( active ? ImGuiCol_SeparatorActive : ImGuiCol_Separator ) );
    ImGui::SameLine( 0.0f, 0.0f );
}
}

AssetBrowserResult DrawBottomPanel(
    asge::ecs::Registry& inRegistry, asge::filesystem::VirtualFileSystem& inVfs,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    SDL_Window* inWindow, bool inHasProject ) noexcept
{
    ImGuiIO const& io = ImGui::GetIO();
    ImGuiStyle const& style = ImGui::GetStyle();
    float const width = io.DisplaySize.x;

    // Minimized: just the header row. Otherwise g_Height, which the user can
    // drag (the width stays locked to the window's, so only the top edge moves).
    float const minimizedHeight = ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
    float const height = g_Minimized ? minimizedHeight : g_Height;

    ImGui::SetNextWindowPos( ImVec2( 0.0f, io.DisplaySize.y - height ), ImGuiCond_Always );
    if ( g_Minimized )
    {
        ImGui::SetNextWindowSize( ImVec2( width, minimizedHeight ), ImGuiCond_Always );
    }
    else
    {
        ImGui::SetNextWindowSize( ImVec2( width, g_Height ), ImGuiCond_FirstUseEver );
        ImGui::SetNextWindowSizeConstraints(
            ImVec2( width, kMinHeight ), ImVec2( width, std::max( kMinHeight, io.DisplaySize.y - kReservedAbove ) ) );
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if ( g_Minimized ) flags |= ImGuiWindowFlags_NoResize;

    AssetBrowserResult result;
    ImGui::Begin( "Bottom Panel", nullptr, flags );

    if ( ImGui::ArrowButton( "##minimize", g_Minimized ? ImGuiDir_Up : ImGuiDir_Down ) ) g_Minimized = !g_Minimized;
    if ( ImGui::IsItemHovered() ) ImGui::SetTooltip( g_Minimized ? "Restore the bottom panel" : "Minimize the bottom panel" );
    ImGui::SameLine();
    ImGui::TextUnformatted( "Assets & Log" );

    if ( !g_Minimized )
    {
        // Picks up this frame's drag-resize (if any) for next frame's anchor.
        // Not read while minimized, or the header-only height would stick.
        g_Height = ImGui::GetWindowSize().y;

        ImGui::Separator();
        float const columnsHeight = std::max( 1.0f, ImGui::GetContentRegionAvail().y );
        float const available = ImGui::GetContentRegionAvail().x - 2.0f * kSplitterWidth;

        // Leave the Log at least a minimum-width column when the window is narrow.
        g_MountsWidth = std::clamp( g_MountsWidth, kMinColumnWidth, std::max( kMinColumnWidth, available - 2.0f * kMinColumnWidth ) );
        g_AssetsWidth = std::clamp( g_AssetsWidth, kMinColumnWidth, std::max( kMinColumnWidth, available - g_MountsWidth - kMinColumnWidth ) );
        float const logWidth = std::max( 1.0f, available - g_MountsWidth - g_AssetsWidth );

        if ( ImGui::BeginChild( "##mounts", ImVec2( g_MountsWidth, columnsHeight ), ImGuiChildFlags_Borders ) )
        {
            DrawMountList( inVfs, inRegistry, inAssets, inRenderer, inWindow, inHasProject, g_Explorer );
        }
        ImGui::EndChild();

        Splitter( "##splitMounts", g_MountsWidth, columnsHeight, available - g_AssetsWidth - kMinColumnWidth );

        if ( ImGui::BeginChild( "##assets", ImVec2( g_AssetsWidth, columnsHeight ), ImGuiChildFlags_Borders ) )
        {
            result = DrawAssetBrowserContents( inRegistry, inVfs, inWindow, inHasProject );
        }
        ImGui::EndChild();

        Splitter( "##splitAssets", g_AssetsWidth, columnsHeight, available - g_MountsWidth - kMinColumnWidth );

        if ( ImGui::BeginChild( "##log", ImVec2( logWidth, columnsHeight ), ImGuiChildFlags_Borders ) )
        {
            DrawConsoleContents( inWindow );
        }
        ImGui::EndChild();
    }

    ImGui::End();
    return result;
}
