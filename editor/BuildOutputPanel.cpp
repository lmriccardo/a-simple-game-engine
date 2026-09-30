#include "BuildOutputPanel.hpp"

#include <imgui.h>

#include "BuildOutput.hpp"

namespace
{

ImVec4 ColorFor( BuildLevel inLevel ) noexcept
{
    switch ( inLevel )
    {
    case BuildLevel::Warning: return ImVec4( 1.0f, 0.8f, 0.2f, 1.0f );
    case BuildLevel::Error:   return ImVec4( 1.0f, 0.35f, 0.35f, 1.0f );
    case BuildLevel::Info:    break;
    }
    return ImVec4( 0.85f, 0.85f, 0.85f, 1.0f );
}

}

void DrawBuildOutputPanel( bool& ioOpen ) noexcept
{
    ImGui::SetNextWindowPos( ImVec2( 40.0f, 60.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 720.0f, 280.0f ), ImGuiCond_FirstUseEver );
    if ( !ImGui::Begin( "Build Output", &ioOpen ) )
    {
        ImGui::End();
        return;
    }

    if ( ImGui::Button( "Clear" ) ) ClearBuildOutput();
    ImGui::Separator();

    ImGui::BeginChild( "BuildOutputScroll", ImVec2( 0.0f, 0.0f ), false, ImGuiWindowFlags_HorizontalScrollbar );
    for ( auto const& line : SnapshotBuildOutput() )
    {
        ImGui::TextColored( ColorFor( line.m_Level ), "%s", line.m_Text.c_str() );
    }
    // Follows new output only while already at the bottom, so scrolling up to read an earlier line sticks.
    if ( ImGui::GetScrollY() >= ImGui::GetScrollMaxY() ) ImGui::SetScrollHereY( 1.0f );
    ImGui::EndChild();
    ImGui::End();
}
