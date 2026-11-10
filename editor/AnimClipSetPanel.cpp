#include "AnimClipSetPanel.hpp"

#include <algorithm>
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
    ImGui::SetNextWindowSize( ImVec2( 640.0f, 360.0f ), ImGuiCond_FirstUseEver );
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
        ImGui::PushID( i );
        if ( declared ) ImGui::TextUnformatted( clip.m_Name.c_str() );
        else ImGui::TextColored( ImVec4( 1.0f, 0.7f, 0.3f, 1.0f ), "%s (not in machine)", clip.m_Name.c_str() );
        ImGui::SameLine( 170.0f );
        ImGui::SetNextItemWidth( 260.0f );
        if ( PathCombo( "##table", clip.m_Path, tables ) ) Touch();
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 80.0f );
        if ( ImGui::InputFloat( "##dur", &clip.m_FrameDuration, 0.0f, 0.0f, "%.3f s" ) ) Touch();
        if ( !declared )
        {
            ImGui::SameLine();
            if ( ImGui::SmallButton( "Remove" ) ) removeAt = i;
        }
        ImGui::PopID();
    }
    if ( removeAt >= 0 ) { g.m_Doc.m_Clips.erase( g.m_Doc.m_Clips.begin() + removeAt ); Touch(); }
    if ( g.m_Doc.m_Clips.empty() ) ImGui::TextDisabled( "The machine declares no clips" );

    ImGui::End();
}
