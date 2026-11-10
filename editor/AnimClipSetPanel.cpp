#include "AnimClipSetPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "AnimGraph.hpp"
#include "AssetBrowser.hpp"
#include "FileDialog.hpp"

namespace
{

namespace fs = std::filesystem;

constexpr float kPi = 3.14159265f;
constexpr std::size_t kMaxVariants = 8; // the engine's limit; its loader is what enforces it
constexpr SDL_DialogFileFilter kSetFilters[]{ { "Clip set (*.animset)", "animset" } };

struct PanelState
{
    AnimClipSetDoc           m_Doc;
    fs::path                 m_Path;
    std::string              m_Status;
    std::string              m_Problem;       // Engine's verdict on the doc; empty = valid
    std::vector<std::string> m_MachineClips;  // Clip names of the picked machine
    std::string              m_MachineError;  // Why the machine could not be read
    bool                     m_Dirty = true;  // Doc changed since the last validation
    bool                     m_Modified = false;
    bool                     m_NeedSync = true;
    FileDialogResult         m_SaveDialog;
};

PanelState g;

void Touch() noexcept { g.m_Dirty = true; g.m_Modified = true; }

bool PathCombo( char const* inLabel, std::string& ioPath, std::vector<std::string> const& inKnown ) noexcept
{
    bool changed = false;
    if ( ImGui::BeginCombo( inLabel, ioPath.empty() ? "(none)" : ioPath.c_str() ) )
    {
        for ( auto const& p : inKnown )
        {
            if ( ImGui::Selectable( p.c_str(), p == ioPath ) ) { ioPath = p; changed = true; }
        }
        if ( inKnown.empty() ) ImGui::TextDisabled( "Nothing known: Add > Load Asset" );
        ImGui::EndCombo();
    }
    return changed;
}

// Reads the picked machine for its clip names and adds a binding for each one the set lacks.
void SyncWithMachine( asge::filesystem::VirtualFileSystem const& inVfs )
{
    g.m_NeedSync = false;
    g.m_MachineClips.clear();
    g.m_MachineError.clear();
    if ( g.m_Doc.m_Machine.empty() ) { g.m_MachineError = "No machine picked"; return; }

    auto const real = inVfs.Resolve( g.m_Doc.m_Machine );
    if ( !real.IsOk() ) { g.m_MachineError = real.Error().String(); return; }
    auto const machine = ReadMachine( real.Value() );
    if ( !machine.IsOk() ) { g.m_MachineError = machine.Error().String(); return; }

    g.m_MachineClips = machine.Value().m_Clips;
    for ( auto const& name : g.m_MachineClips )
    {
        bool const bound = std::ranges::any_of( g.m_Doc.m_Clips, [&]( AnimClipBinding const& b ) { return b.m_Name == name; } );
        if ( bound ) continue;
        AnimClipBinding b;
        b.m_Name = name;
        b.m_Variants.emplace_back();
        g.m_Doc.m_Clips.push_back( std::move( b ) );
        Touch();
    }
}

void Validate()
{
    if ( !g.m_Dirty ) return;
    g.m_Dirty = false;
    auto const result = ValidateClipSet( g.m_Doc );
    g.m_Problem = result.IsOk() ? std::string{} : result.Error().String();
}

void SaveTo( fs::path const& inPath )
{
    auto const written = WriteClipSet( g.m_Doc, inPath );
    if ( !written.IsOk() ) { g.m_Status = "Save refused: " + written.Error().String(); return; }
    g.m_Path = inPath;
    g.m_Modified = false;
    g.m_Status = "Saved " + inPath.filename().string();
}

// Compass name for 4 or 8 directions, a bearing in degrees clockwise from north otherwise.
std::string DirectionLabel( int inIndex, int inCount )
{
    static char const* const kNames[]{ "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    if ( inCount <= 1 ) return "";
    if ( inCount == 4 ) return kNames[inIndex * 2];
    if ( inCount == 8 ) return kNames[inIndex];

    char buf[32];
    std::snprintf( buf, sizeof buf, "%.0f deg", 360.0f * static_cast<float>( inIndex ) / static_cast<float>( inCount ) );
    return buf;
}

std::string ShortName( std::string const& inPath, std::size_t inMax )
{
    if ( inPath.empty() ) return "-";
    std::string stem = fs::path( inPath ).stem().string();
    if ( stem.size() > inMax ) stem = stem.substr( 0, inMax - 1 ) + "~";
    return stem;
}

// A circle cut into one sector per variant, north at the top and clockwise, each showing its direction and frame table.
void DrawDirectionWheel( AnimClipBinding const& inClip )
{
    constexpr float kSize = 190.0f;
    int const n = static_cast<int>( inClip.m_Variants.size() );
    ImVec2 const origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy( ImVec2( kSize, kSize ) );

    ImDrawList* const list = ImGui::GetWindowDrawList();
    ImVec2 const center( origin.x + kSize * 0.5f, origin.y + kSize * 0.5f );
    float const radius = kSize * 0.5f - 2.0f;
    float const half = kPi / static_cast<float>( n );

    int hovered = -1;
    if ( ImGui::IsItemHovered() )
    {
        ImVec2 const m = ImGui::GetMousePos();
        float const dx = m.x - center.x, dy = m.y - center.y;
        if ( dx * dx + dy * dy <= radius * radius )
        {
            float bearing = std::atan2( dx, -dy );
            if ( bearing < 0.0f ) bearing += 2.0f * kPi;
            hovered = static_cast<int>( std::lround( bearing / ( 2.0f * half ) ) ) % n;
        }
    }

    std::size_t const maxChars = n <= 2 ? 14 : n <= 4 ? 9 : 6;
    for ( int i = 0; i < n; ++i )
    {
        float const mid = 2.0f * half * static_cast<float>( i ) - kPi * 0.5f; // ImGui angles: 0 is east, clockwise
        bool const empty = inClip.m_Variants[i].empty();
        float const value = empty ? 0.22f : ( i == hovered ? 0.85f : 0.6f );
        ImU32 const fill = ImColor::HSV( static_cast<float>( i ) / static_cast<float>( n ), empty ? 0.0f : 0.5f, value );

        list->PathLineTo( center );
        list->PathArcTo( center, radius, mid - half, mid + half );
        list->PathFillConvex( fill );
        list->AddLine( center, ImVec2( center.x + std::cos( mid - half ) * radius, center.y + std::sin( mid - half ) * radius ),
                       IM_COL32( 20, 20, 24, 255 ), 1.5f );

        ImVec2 const dir( std::cos( mid ), std::sin( mid ) );
        float const at = n == 1 ? 0.0f : 0.62f;
        auto const draw = [&]( std::string const& inText, float inRadius, ImU32 inColor )
        {
            ImVec2 const size = ImGui::CalcTextSize( inText.c_str() );
            list->AddText( ImVec2( center.x + dir.x * radius * inRadius - size.x * 0.5f, center.y + dir.y * radius * inRadius - size.y * 0.5f ),
                           inColor, inText.c_str() );
        };
        draw( DirectionLabel( i, n ), 0.85f, IM_COL32( 255, 255, 255, 255 ) );
        draw( ShortName( inClip.m_Variants[i], maxChars ), at, IM_COL32( 235, 235, 235, 255 ) );
    }
    list->AddCircle( center, radius, IM_COL32( 20, 20, 24, 255 ), 0, 2.0f );

    if ( hovered >= 0 )
    {
        auto const& path = inClip.m_Variants[hovered];
        ImGui::SetTooltip( "%s: %s", DirectionLabel( hovered, n ).c_str(), path.empty() ? "(no frame table)" : path.c_str() );
    }
}

bool DeclaredByMachine( std::string const& inName ) noexcept
{
    return std::ranges::find( g.m_MachineClips, inName ) != g.m_MachineClips.end();
}

}

void OpenAnimClipSet( fs::path const& inPath ) noexcept
{
    auto doc = ReadClipSet( inPath );
    if ( !doc.IsOk() ) { g.m_Status = "Open failed: " + doc.Error().String(); return; }
    g.m_Doc = std::move( doc.Value() );
    g.m_Path = inPath;
    g.m_Modified = false;
    g.m_Dirty = true;
    g.m_NeedSync = true;
    g.m_Status = "Opened " + inPath.filename().string();
}

void DrawAnimClipSetPanel(
    bool& ioOpen, SDL_Window* inWindow, asge::filesystem::VirtualFileSystem const& inVfs,
    asge::ecs::Registry& inRegistry ) noexcept
{
    std::string chosen;
    if ( DrainFileDialogResult( g.m_SaveDialog, chosen ) && !chosen.empty() )
    {
        fs::path p = chosen;
        if ( p.extension() != ".animset" ) p += ".animset";
        SaveTo( p );
    }

    ImGui::SetNextWindowPos( ImVec2( 80.0f, 100.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 780.0f, 480.0f ), ImGuiCond_FirstUseEver );
    if ( !ImGui::Begin( "Animation Clip Set", &ioOpen ) ) { ImGui::End(); return; }

    if ( g.m_NeedSync ) SyncWithMachine( inVfs );
    Validate();

    if ( ImGui::Button( "Save" ) )
    {
        if ( g.m_Path.empty() ) SDL_ShowSaveFileDialog( OnFileDialogResult, &g.m_SaveDialog, inWindow, kSetFilters, 1, nullptr );
        else SaveTo( g.m_Path );
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Save As..." ) )
        SDL_ShowSaveFileDialog( OnFileDialogResult, &g.m_SaveDialog, inWindow, kSetFilters, 1, nullptr );
    ImGui::SameLine();
    ImGui::TextDisabled( "%s%s  %s", g.m_Path.empty() ? "(unsaved)" : g.m_Path.string().c_str(), g.m_Modified ? " *" : "", g.m_Status.c_str() );

    if ( g.m_Problem.empty() ) ImGui::TextColored( ImVec4( 0.4f, 0.9f, 0.5f, 1.0f ), "Valid" );
    else
    {
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ) );
        ImGui::TextWrapped( "%s", g.m_Problem.c_str() );
        ImGui::PopStyleColor();
    }
    ImGui::Separator();

    if ( PathCombo( "Machine", g.m_Doc.m_Machine, KnownMachinePaths() ) ) { g.m_NeedSync = true; Touch(); }
    if ( !g.m_MachineError.empty() )
        ImGui::TextColored( ImVec4( 1.0f, 0.7f, 0.3f, 1.0f ), "%s", g.m_MachineError.c_str() );

    ImGui::SeparatorText( "Clips" );
    auto const tables = KnownAnimationPaths( inRegistry );
    int removeAt = -1;
    for ( int i = 0; i < static_cast<int>( g.m_Doc.m_Clips.size() ); ++i )
    {
        auto& clip = g.m_Doc.m_Clips[i];
        bool const declared = DeclaredByMachine( clip.m_Name );
        float const rowWidth = ImGui::GetContentRegionAvail().x;
        float const gap = ImGui::GetStyle().ItemSpacing.x;
        ImGui::PushID( i );
        if ( declared ) ImGui::TextUnformatted( clip.m_Name.c_str() );
        else ImGui::TextColored( ImVec4( 1.0f, 0.7f, 0.3f, 1.0f ), "%s (not in machine)", clip.m_Name.c_str() );
        if ( clip.m_Variants.empty() ) clip.m_Variants.emplace_back();

        ImGui::SameLine( 170.0f );
        ImGui::SetNextItemWidth( 100.0f );
        int const count = static_cast<int>( clip.m_Variants.size() );
        char preview[24];
        std::snprintf( preview, sizeof preview, count == 1 ? "%d direction" : "%d directions", count );
        if ( ImGui::BeginCombo( "##dirs", preview ) )
        {
            for ( int n = 1; n <= static_cast<int>( kMaxVariants ); ++n )
            {
                char item[24];
                std::snprintf( item, sizeof item, n == 1 ? "%d direction" : "%d directions", n );
                if ( ImGui::Selectable( item, n == count ) ) { clip.m_Variants.resize( n ); Touch(); }
            }
            ImGui::EndCombo();
        }

        if ( count == 1 )
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth( std::max( 150.0f, rowWidth - 170.0f - 100.0f - 80.0f - ( declared ? 0.0f : 70.0f ) - gap * 4.0f ) );
            if ( PathCombo( "##table", clip.m_Variants.front(), tables ) ) Touch();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth( 80.0f );
        if ( ImGui::InputFloat( "##dur", &clip.m_FrameDuration, 0.0f, 0.0f, "%.3f s" ) ) Touch();
        if ( !declared )
        {
            ImGui::SameLine();
            if ( ImGui::SmallButton( "Remove" ) ) removeAt = i;
        }

        if ( count > 1 )
        {
            ImGui::Indent();
            ImGui::BeginGroup();
            for ( int v = 0; v < count; ++v )
            {
                ImGui::PushID( v );
                ImGui::SetNextItemWidth( std::max( 200.0f, rowWidth - ImGui::GetStyle().IndentSpacing - 190.0f - 70.0f - gap * 3.0f ) );
                if ( PathCombo( "##table", clip.m_Variants[v], tables ) ) Touch();
                ImGui::SameLine();
                ImGui::TextDisabled( "%s", DirectionLabel( v, count ).c_str() );
                ImGui::PopID();
            }
            ImGui::EndGroup();
            ImGui::SameLine();
            DrawDirectionWheel( clip );
            ImGui::Unindent();
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if ( removeAt >= 0 ) { g.m_Doc.m_Clips.erase( g.m_Doc.m_Clips.begin() + removeAt ); Touch(); }
    if ( g.m_Doc.m_Clips.empty() ) ImGui::TextDisabled( "The machine declares no clips" );

    ImGui::End();
}
