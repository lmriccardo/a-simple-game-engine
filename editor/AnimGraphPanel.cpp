#include "AnimGraphPanel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "AnimGraph.hpp"
#include "FileDialog.hpp"

namespace
{

namespace fs = std::filesystem;

constexpr SDL_DialogFileFilter kMachineFilters[]{ { "State machine (*.animsm)", "animsm" } };
constexpr ImVec2 kNodeSize{ 140.0f, 44.0f };

struct PanelState
{
    AnimMachineDoc  m_Doc;
    AnimLayout      m_Layout;
    fs::path        m_Path;
    std::string     m_Selected;
    std::string     m_Status;
    std::string     m_Problem;       // Engine's verdict on the current doc; empty = valid
    ImVec2          m_Pan{ 20.0f, 20.0f };
    ImVec2          m_MenuPos{};     // Canvas position of the last right click
    bool            m_Dirty = true;  // Doc changed since the last validation
    bool            m_Modified = false;
    bool            m_Preview = false;
    asge::game::components::Animator*               m_LiveAnimator = nullptr;
    asge::game::asset::AnimationStateMachine const* m_LiveMachine = nullptr;  // Set only if the animator plays this doc's machine
    std::string     m_LiveState;      // Name of the animator's current state
    FileDialogResult m_SaveDialog;
};

PanelState g;

void Touch() noexcept { g.m_Dirty = true; g.m_Modified = true; }

bool EditText( char const* inLabel, std::string& ioText ) noexcept
{
    char buf[128];
    std::snprintf( buf, sizeof buf, "%s", ioText.c_str() );
    if ( !ImGui::InputText( inLabel, buf, sizeof buf ) ) return false;
    ioText = buf;
    return true;
}

bool NameCombo( char const* inLabel, std::string& ioName, std::vector<std::string> const& inNames, bool inAllowNone ) noexcept
{
    bool changed = false;
    if ( ImGui::BeginCombo( inLabel, ioName.empty() ? "(none)" : ioName.c_str() ) )
    {
        if ( inAllowNone && ImGui::Selectable( "(none)", ioName.empty() ) ) { ioName.clear(); changed = true; }
        for ( auto const& n : inNames )
        {
            if ( ImGui::Selectable( n.c_str(), n == ioName ) ) { ioName = n; changed = true; }
        }
        ImGui::EndCombo();
    }
    return changed;
}

std::vector<std::string> StateNames()
{
    std::vector<std::string> names;
    for ( auto const& s : g.m_Doc.m_States ) names.push_back( s.m_Name );
    return names;
}

AnimState* FindState( std::string const& inName ) noexcept
{
    for ( auto& s : g.m_Doc.m_States ) if ( s.m_Name == inName ) return &s;
    return nullptr;
}

std::string UniqueStateName()
{
    for ( int i = 1;; ++i )
    {
        std::string name = i == 1 ? "State" : "State" + std::to_string( i );
        if ( !FindState( name ) ) return name;
    }
}

void Validate()
{
    if ( !g.m_Dirty ) return;
    g.m_Dirty = false;
    auto const result = ValidateMachine( g.m_Doc );
    g.m_Problem = result.IsOk() ? std::string{} : result.Error().String();
}

void LoadFrom( fs::path const& inPath )
{
    auto doc = ReadMachine( inPath );
    if ( !doc.IsOk() ) { g.m_Status = "Open failed: " + doc.Error().String(); return; }
    g.m_Doc = std::move( doc.Value() );
    g.m_Path = inPath;
    g.m_Layout = LoadLayout( inPath );
    AutoLayout( g.m_Doc, g.m_Layout );
    g.m_Selected.clear();
    g.m_Modified = false;
    g.m_Dirty = true;
    g.m_Status = "Opened " + inPath.filename().string();
}

void SaveTo( fs::path const& inPath )
{
    AutoLayout( g.m_Doc, g.m_Layout );
    auto const written = WriteMachine( g.m_Doc, inPath );
    if ( !written.IsOk() ) { g.m_Status = "Save refused: " + written.Error().String(); return; }
    g.m_Path = inPath;
    auto const layoutSaved = SaveLayout( inPath, g.m_Layout );
    g.m_Modified = false;
    g.m_Status = layoutSaved.IsOk() ? "Saved " + inPath.filename().string() : "Saved, but layout failed: " + layoutSaved.Error().String();
}

template<typename Fn>
void ForEachRef( Fn&& inFn )
{
    for ( auto& s : g.m_Doc.m_States ) for ( auto& t : s.m_Transitions ) inFn( t );
}

void RenameState( std::string const& inOld, std::string const& inNew )
{
    if ( inOld == inNew ) return;
    if ( g.m_Doc.m_Initial == inOld ) g.m_Doc.m_Initial = inNew;
    ForEachRef( [&]( AnimTransition& t ) { if ( t.m_To == inOld ) t.m_To = inNew; } );
    if ( auto it = g.m_Layout.find( inOld ); it != g.m_Layout.end() )
    {
        auto const pos = it->second;
        g.m_Layout.erase( it );
        g.m_Layout[inNew] = pos;
    }
    if ( g.m_Selected == inOld ) g.m_Selected = inNew;
}

void DeleteState( std::string const& inName )
{
    std::erase_if( g.m_Doc.m_States, [&]( AnimState const& s ) { return s.m_Name == inName; } );
    for ( auto& s : g.m_Doc.m_States ) std::erase_if( s.m_Transitions, [&]( AnimTransition const& t ) { return t.m_To == inName; } );
    if ( g.m_Doc.m_Initial == inName ) g.m_Doc.m_Initial = g.m_Doc.m_States.empty() ? std::string{} : g.m_Doc.m_States.front().m_Name;
    g.m_Layout.erase( inName );
    if ( g.m_Selected == inName ) g.m_Selected.clear();
}

void AddState( ImVec2 inPos )
{
    AnimState s;
    s.m_Name = UniqueStateName();
    if ( !g.m_Doc.m_Clips.empty() ) s.m_Clip = g.m_Doc.m_Clips.front();
    g.m_Layout[s.m_Name] = AnimNodePos{ inPos.x, inPos.y };
    if ( g.m_Doc.m_States.empty() ) g.m_Doc.m_Initial = s.m_Name;
    g.m_Selected = s.m_Name;
    g.m_Doc.m_States.push_back( std::move( s ) );
}

// Renames/removes one entry of a flag, trigger or clip list, keeping the references in step.
enum class ListKind { Flag, Trigger, Clip };

void RenameEntry( ListKind inKind, std::string const& inOld, std::string const& inNew )
{
    auto swap = [&]( std::string& s ) { if ( s == inOld ) s = inNew; };
    if ( inKind == ListKind::Clip ) { for ( auto& s : g.m_Doc.m_States ) swap( s.m_Clip ); return; }
    ForEachRef( [&]( AnimTransition& t )
    {
        if ( inKind == ListKind::Trigger ) { swap( t.m_Trigger ); return; }
        for ( auto& f : t.m_FlagsSet ) swap( f );
        for ( auto& f : t.m_FlagsClear ) swap( f );
    } );
}

void RemoveEntry( ListKind inKind, std::string const& inName )
{
    if ( inKind == ListKind::Clip ) { for ( auto& s : g.m_Doc.m_States ) if ( s.m_Clip == inName ) s.m_Clip.clear(); return; }
    ForEachRef( [&]( AnimTransition& t )
    {
        if ( inKind == ListKind::Trigger ) { if ( t.m_Trigger == inName ) t.m_Trigger.clear(); return; }
        std::erase( t.m_FlagsSet, inName );
        std::erase( t.m_FlagsClear, inName );
    } );
}

void DrawNameList( char const* inTitle, ListKind inKind, std::vector<std::string>& ioList )
{
    ImGui::PushID( inTitle );
    if ( ImGui::CollapsingHeader( inTitle ) )
    {
        int removeAt = -1;
        for ( int i = 0; i < static_cast<int>( ioList.size() ); ++i )
        {
            ImGui::PushID( i );
            std::string const before = ioList[i];
            ImGui::SetNextItemWidth( 180.0f );
            if ( EditText( "##n", ioList[i] ) ) { RenameEntry( inKind, before, ioList[i] ); Touch(); }
            ImGui::SameLine();
            if ( ImGui::SmallButton( "x" ) ) removeAt = i;
            ImGui::PopID();
        }
        if ( removeAt >= 0 )
        {
            RemoveEntry( inKind, ioList[removeAt] );
            ioList.erase( ioList.begin() + removeAt );
            Touch();
        }
        if ( ImGui::SmallButton( "+ Add" ) )
        {
            std::string name;
            for ( int i = static_cast<int>( ioList.size() ) + 1;; ++i )
            {
                name = std::string( inTitle ).substr( 0, std::string( inTitle ).size() - 1 ) + std::to_string( i );
                if ( std::find( ioList.begin(), ioList.end(), name ) == ioList.end() ) break;
            }
            ioList.push_back( name );
            Touch();
        }
    }
    ImGui::PopID();
}

// Tri-state per machine flag: ignored, must be set, must be clear.
void DrawFlagsPopup( AnimTransition& ioT )
{
    for ( auto const& f : g.m_Doc.m_Flags )
    {
        int mode = std::find( ioT.m_FlagsSet.begin(), ioT.m_FlagsSet.end(), f ) != ioT.m_FlagsSet.end() ? 1
                 : std::find( ioT.m_FlagsClear.begin(), ioT.m_FlagsClear.end(), f ) != ioT.m_FlagsClear.end() ? 2 : 0;
        int const before = mode;
        ImGui::PushID( f.c_str() );
        ImGui::SetNextItemWidth( 90.0f );
        ImGui::Combo( "##m", &mode, "any\0set\0clear\0" );
        ImGui::SameLine();
        ImGui::TextUnformatted( f.c_str() );
        ImGui::PopID();
        if ( mode == before ) continue;
        std::erase( ioT.m_FlagsSet, f );
        std::erase( ioT.m_FlagsClear, f );
        if ( mode == 1 ) ioT.m_FlagsSet.push_back( f );
        if ( mode == 2 ) ioT.m_FlagsClear.push_back( f );
        Touch();
    }
    if ( g.m_Doc.m_Flags.empty() ) ImGui::TextDisabled( "No flags declared" );
}

void DrawStateEditor( AnimState& ioState )
{
    std::string const oldName = ioState.m_Name;
    ImGui::SetNextItemWidth( 180.0f );
    if ( EditText( "Name", ioState.m_Name ) ) { RenameState( oldName, ioState.m_Name ); Touch(); }
    ImGui::SetNextItemWidth( 180.0f );
    if ( NameCombo( "Clip", ioState.m_Clip, g.m_Doc.m_Clips, true ) ) Touch();
    int end = static_cast<int>( ioState.m_End );
    ImGui::SetNextItemWidth( 180.0f );
    if ( ImGui::Combo( "End", &end, "loop\0hold\0restore\0" ) ) { ioState.m_End = static_cast<AnimEnd>( end ); Touch(); }
    bool isInitial = g.m_Doc.m_Initial == ioState.m_Name;
    if ( ImGui::Checkbox( "Is initial", &isInitial ) && isInitial ) { g.m_Doc.m_Initial = ioState.m_Name; Touch(); }

    ImGui::SeparatorText( "Transitions" );
    int removeAt = -1;
    auto const states = StateNames();
    for ( int i = 0; i < static_cast<int>( ioState.m_Transitions.size() ); ++i )
    {
        auto& t = ioState.m_Transitions[i];
        ImGui::PushID( i );
        ImGui::SetNextItemWidth( 150.0f );
        if ( NameCombo( "To", t.m_To, states, false ) ) Touch();
        ImGui::SetNextItemWidth( 150.0f );
        if ( NameCombo( "Trigger", t.m_Trigger, g.m_Doc.m_Triggers, true ) ) Touch();
        if ( ImGui::Checkbox( "At end", &t.m_AtEnd ) ) Touch();
        ImGui::SetNextItemWidth( 90.0f );
        ImGui::SameLine();
        if ( ImGui::InputInt( "Prio", &t.m_Priority ) ) Touch();
        if ( ImGui::SmallButton( "Flags..." ) ) ImGui::OpenPopup( "flags" );
        ImGui::SameLine();
        if ( ImGui::SmallButton( "Delete" ) ) removeAt = i;
        if ( ImGui::BeginPopup( "flags" ) ) { DrawFlagsPopup( t ); ImGui::EndPopup(); }
        ImGui::Separator();
        ImGui::PopID();
    }
    if ( removeAt >= 0 ) { ioState.m_Transitions.erase( ioState.m_Transitions.begin() + removeAt ); Touch(); }
    if ( ImGui::SmallButton( "+ Transition" ) )
    {
        AnimTransition t;
        t.m_To = ioState.m_Name;
        ioState.m_Transitions.push_back( std::move( t ) );
        Touch();
    }
}

ImVec2 BorderPoint( ImVec2 inCenter, ImVec2 inToward ) noexcept
{
    float const dx = inToward.x - inCenter.x, dy = inToward.y - inCenter.y;
    float const ax = std::abs( dx ) < 1e-3f ? 1e-3f : std::abs( dx );
    float const ay = std::abs( dy ) < 1e-3f ? 1e-3f : std::abs( dy );
    float const t = std::min( kNodeSize.x * 0.5f / ax, kNodeSize.y * 0.5f / ay );
    return ImVec2( inCenter.x + dx * t, inCenter.y + dy * t );
}

std::string EdgeLabel( AnimTransition const& inT )
{
    std::string label = inT.m_Trigger;
    auto add = [&]( std::string const& part ) { if ( !label.empty() ) label += " "; label += part; };
    if ( inT.m_AtEnd ) add( "@end" );
    for ( auto const& f : inT.m_FlagsSet ) add( f );
    for ( auto const& f : inT.m_FlagsClear ) add( "!" + f );
    return label;
}

void DrawArrow( ImDrawList* inList, ImVec2 inFrom, ImVec2 inTo, ImU32 inCol, std::string const& inLabel )
{
    inList->AddLine( inFrom, inTo, inCol, 1.5f );
    float const dx = inTo.x - inFrom.x, dy = inTo.y - inFrom.y;
    float const len = std::sqrt( dx * dx + dy * dy );
    if ( len < 1.0f ) return;
    ImVec2 const d( dx / len, dy / len ), n( -d.y, d.x );
    inList->AddTriangleFilled( inTo, ImVec2( inTo.x - d.x * 10 + n.x * 5, inTo.y - d.y * 10 + n.y * 5 ),
                               ImVec2( inTo.x - d.x * 10 - n.x * 5, inTo.y - d.y * 10 - n.y * 5 ), inCol );
    if ( !inLabel.empty() )
        inList->AddText( ImVec2( ( inFrom.x + inTo.x ) * 0.5f + n.x * 4, ( inFrom.y + inTo.y ) * 0.5f + n.y * 4 ), inCol, inLabel.c_str() );
}

void DrawCanvas()
{
    ImGui::BeginChild( "canvas", ImVec2( ImGui::GetContentRegionAvail().x - 330.0f, 0.0f ), true,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove );
    ImDrawList* list = ImGui::GetWindowDrawList();
    ImVec2 const origin = ImGui::GetCursorScreenPos();
    ImVec2 const canvasSize = ImGui::GetContentRegionAvail();

    // Background interaction: pan with drag, deselect on click, add-state menu on right click.
    ImGui::SetCursorScreenPos( origin );
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton( "bg", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle );
    if ( ImGui::IsItemActive() && ( ImGui::IsMouseDragging( ImGuiMouseButton_Left ) || ImGui::IsMouseDragging( ImGuiMouseButton_Middle ) ) )
    {
        g.m_Pan.x += ImGui::GetIO().MouseDelta.x;
        g.m_Pan.y += ImGui::GetIO().MouseDelta.y;
    }
    if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) ) g.m_Selected.clear();
    if ( ImGui::IsItemClicked( ImGuiMouseButton_Right ) )
    {
        ImVec2 const m = ImGui::GetMousePos();
        g.m_MenuPos = ImVec2( m.x - origin.x - g.m_Pan.x, m.y - origin.y - g.m_Pan.y );
        ImGui::OpenPopup( "canvasMenu" );
    }
    if ( ImGui::BeginPopup( "canvasMenu" ) )
    {
        if ( ImGui::MenuItem( "Add State" ) ) { AddState( g.m_MenuPos ); Touch(); }
        ImGui::EndPopup();
    }

    auto center = [&]( std::string const& inName ) -> ImVec2
    {
        auto const it = g.m_Layout.find( inName );
        AnimNodePos const p = it == g.m_Layout.end() ? AnimNodePos{} : it->second;
        return ImVec2( origin.x + g.m_Pan.x + p.m_X + kNodeSize.x * 0.5f, origin.y + g.m_Pan.y + p.m_Y + kNodeSize.y * 0.5f );
    };

    ImU32 const edgeCol = ImGui::GetColorU32( ImGuiCol_Text, 0.75f );
    for ( auto const& s : g.m_Doc.m_States )
    {
        for ( auto const& t : s.m_Transitions )
        {
            if ( !FindState( t.m_To ) ) continue;
            ImVec2 const a = center( s.m_Name ), b = center( t.m_To );
            if ( s.m_Name == t.m_To )
            {
                ImVec2 const top( a.x, a.y - kNodeSize.y * 0.5f );
                list->AddCircle( ImVec2( top.x, top.y - 12.0f ), 12.0f, edgeCol, 24, 1.5f );
                list->AddText( ImVec2( top.x + 14.0f, top.y - 28.0f ), edgeCol, EdgeLabel( t ).c_str() );
                continue;
            }
            // Offset so a pair of opposite edges does not overlap.
            float const dx = b.x - a.x, dy = b.y - a.y, len = std::max( std::sqrt( dx * dx + dy * dy ), 1.0f );
            ImVec2 const off( -dy / len * 7.0f, dx / len * 7.0f );
            ImVec2 const a2( a.x + off.x, a.y + off.y ), b2( b.x + off.x, b.y + off.y );
            DrawArrow( list, BorderPoint( a2, b2 ), BorderPoint( b2, a2 ), edgeCol, EdgeLabel( t ) );
        }
    }

    std::string deleteName;
    for ( auto& s : g.m_Doc.m_States )
    {
        ImVec2 const c = center( s.m_Name );
        ImVec2 const tl( c.x - kNodeSize.x * 0.5f, c.y - kNodeSize.y * 0.5f );
        ImGui::SetCursorScreenPos( tl );
        ImGui::PushID( s.m_Name.c_str() );
        ImGui::InvisibleButton( "node", kNodeSize );
        if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) || ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) g.m_Selected = s.m_Name;
        if ( ImGui::IsItemActive() && ImGui::IsMouseDragging( ImGuiMouseButton_Left ) )
        {
            auto& p = g.m_Layout[s.m_Name];
            p.m_X += ImGui::GetIO().MouseDelta.x;
            p.m_Y += ImGui::GetIO().MouseDelta.y;
            g.m_Modified = true;
        }
        if ( ImGui::BeginPopupContextItem( "nodeMenu", ImGuiPopupFlags_MouseButtonRight ) )
        {
            if ( ImGui::MenuItem( "Set Initial" ) ) { g.m_Doc.m_Initial = s.m_Name; Touch(); }
            if ( ImGui::BeginMenu( "Add Transition To" ) )
            {
                for ( auto const& other : g.m_Doc.m_States )
                {
                    if ( !ImGui::MenuItem( other.m_Name.c_str() ) ) continue;
                    AnimTransition t;
                    t.m_To = other.m_Name;
                    s.m_Transitions.push_back( std::move( t ) );
                    Touch();
                }
                ImGui::EndMenu();
            }
            if ( ImGui::MenuItem( "Delete" ) ) deleteName = s.m_Name;
            ImGui::EndPopup();
        }
        ImGui::PopID();

        bool const live = g.m_LiveMachine && s.m_Name == g.m_LiveState;
        bool const selected = s.m_Name == g.m_Selected;
        ImU32 const fill = ImGui::GetColorU32( selected ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg );
        list->AddRectFilled( tl, ImVec2( tl.x + kNodeSize.x, tl.y + kNodeSize.y ), fill, 6.0f );
        list->AddRect( tl, ImVec2( tl.x + kNodeSize.x, tl.y + kNodeSize.y ),
                       selected ? IM_COL32( 255, 200, 60, 255 ) : ImGui::GetColorU32( ImGuiCol_Border ), 6.0f, 0, selected ? 2.5f : 1.0f );
        if ( live )
            list->AddRect( ImVec2( tl.x - 5, tl.y - 5 ), ImVec2( tl.x + kNodeSize.x + 5, tl.y + kNodeSize.y + 5 ), IM_COL32( 80, 200, 255, 255 ), 9.0f, 0, 3.0f );
        if ( s.m_Name == g.m_Doc.m_Initial )
            list->AddRect( ImVec2( tl.x - 3, tl.y - 3 ), ImVec2( tl.x + kNodeSize.x + 3, tl.y + kNodeSize.y + 3 ), IM_COL32( 90, 200, 120, 255 ), 8.0f, 0, 1.5f );
        list->AddText( ImVec2( tl.x + 8, tl.y + 6 ), ImGui::GetColorU32( ImGuiCol_Text ), s.m_Name.c_str() );
        std::string const sub = ( s.m_Clip.empty() ? "(no clip)" : s.m_Clip ) + ( s.m_End == AnimEnd::Loop ? "" : s.m_End == AnimEnd::Hold ? " [hold]" : " [restore]" );
        list->AddText( ImVec2( tl.x + 8, tl.y + 24 ), ImGui::GetColorU32( ImGuiCol_TextDisabled ), sub.c_str() );
    }
    if ( !deleteName.empty() ) { DeleteState( deleteName ); Touch(); }
    ImGui::EndChild();
}

void DrawLive()
{
    ImGui::SeparatorText( "Live" );
    if ( !g.m_Preview ) { ImGui::TextDisabled( "Tick Preview to run the animations" ); return; }
    if ( !g.m_LiveAnimator ) { ImGui::TextDisabled( "Select an entity with an Animator" ); return; }
    if ( !g.m_LiveMachine ) { ImGui::TextDisabled( "The selected Animator does not use this machine" ); return; }

    auto& a = *g.m_LiveAnimator;
    char const* status = a.m_Status == asge::game::components::AnimStatus::Playing ? "playing"
                       : a.m_Status == asge::game::components::AnimStatus::Stopped ? "stopped" : "finished";
    ImGui::Text( "State: %s  frame %u  %s", g.m_LiveState.c_str(), a.m_Frame, status );
    if ( ImGui::SmallButton( "Stop" ) ) asge::game::components::Stop( a );
    ImGui::SameLine();
    if ( ImGui::SmallButton( "Resume" ) ) asge::game::components::Resume( a );

    for ( auto const& f : g.m_Doc.m_Flags )
    {
        auto const id = g.m_LiveMachine->FindFlag( f );
        if ( !id.IsOk() ) continue;
        bool on = ( a.m_Flags >> id.Value() ) & 1u;
        if ( ImGui::Checkbox( f.c_str(), &on ) ) asge::game::components::SetFlag( a, f, on );
    }
    for ( auto const& t : g.m_Doc.m_Triggers )
    {
        auto const id = g.m_LiveMachine->FindTrigger( t );
        if ( !id.IsOk() ) continue;
        bool const pending = ( a.m_Triggers >> id.Value() ) & 1u;
        if ( pending ) ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.7f, 0.5f, 0.1f, 1.0f ) );
        if ( ImGui::Button( t.c_str() ) ) asge::game::components::Fire( a, t );
        if ( pending ) ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

void DrawSide()
{
    ImGui::BeginChild( "side", ImVec2( 0.0f, 0.0f ), true );
    if ( g.m_Problem.empty() ) ImGui::TextColored( ImVec4( 0.4f, 0.9f, 0.5f, 1.0f ), "Valid" );
    else
    {
        ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ) );
        ImGui::TextWrapped( "%s", g.m_Problem.c_str() );
        ImGui::PopStyleColor();
    }
    DrawLive();
    ImGui::Separator();

    auto const states = StateNames();
    ImGui::SetNextItemWidth( 180.0f );
    if ( NameCombo( "Initial", g.m_Doc.m_Initial, states, true ) ) Touch();
    DrawNameList( "Flags", ListKind::Flag, g.m_Doc.m_Flags );
    DrawNameList( "Triggers", ListKind::Trigger, g.m_Doc.m_Triggers );
    DrawNameList( "Clips", ListKind::Clip, g.m_Doc.m_Clips );

    ImGui::SeparatorText( "Selected state" );
    if ( AnimState* s = FindState( g.m_Selected ) ) DrawStateEditor( *s );
    else ImGui::TextDisabled( "Click a state" );
    ImGui::EndChild();
}

}

void OpenAnimGraph( fs::path const& inPath ) noexcept { LoadFrom( inPath ); }

void DrawAnimGraphPanel(
    bool& ioOpen, SDL_Window* inWindow, bool& ioPreview, asge::game::components::Animator* inAnimator,
    asge::filesystem::VirtualFileSystem const& inVfs ) noexcept
{
    std::string chosen;
    if ( DrainFileDialogResult( g.m_SaveDialog, chosen ) && !chosen.empty() )
    {
        fs::path p = chosen;
        if ( p.extension() != ".animsm" ) p += ".animsm";
        SaveTo( p );
    }

    ImGui::SetNextWindowPos( ImVec2( 60.0f, 80.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 1000.0f, 560.0f ), ImGuiCond_FirstUseEver );
    if ( !ImGui::Begin( "Animation Graph", &ioOpen ) ) { ImGui::End(); return; }

    Validate();

    g.m_Preview = ioPreview;
    g.m_LiveAnimator = inAnimator;
    g.m_LiveMachine = nullptr;
    if ( ioPreview && inAnimator && inAnimator->m_ClipSet && inAnimator->m_ClipSet->Get().IsResolved() && !g.m_Path.empty() )
    {
        auto const& set = inAnimator->m_ClipSet->Get();
        auto const real = inVfs.Resolve( asge::str::String( set.MachinePath() ) );
        std::error_code ec;
        if ( real.IsOk() && fs::equivalent( real.Value(), g.m_Path, ec ) )
        {
            g.m_LiveMachine = &set.Machine();
            g.m_LiveState.clear();
            for ( auto const& s : g.m_Doc.m_States )
            {
                auto const id = g.m_LiveMachine->FindState( s.m_Name );
                if ( id.IsOk() && id.Value() == inAnimator->m_State ) g.m_LiveState = s.m_Name;
            }
        }
    }

    if ( ImGui::Button( "New" ) )
    {
        g.m_Doc = {}; g.m_Layout.clear(); g.m_Path.clear(); g.m_Selected.clear();
        g.m_Modified = false; g.m_Dirty = true; g.m_Status = "New machine";
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Save" ) )
    {
        if ( g.m_Path.empty() ) SDL_ShowSaveFileDialog( OnFileDialogResult, &g.m_SaveDialog, inWindow, kMachineFilters, 1, nullptr );
        else SaveTo( g.m_Path );
    }
    ImGui::SameLine();
    if ( ImGui::Button( "Save As..." ) )
        SDL_ShowSaveFileDialog( OnFileDialogResult, &g.m_SaveDialog, inWindow, kMachineFilters, 1, nullptr );
    ImGui::SameLine();
    ImGui::TextDisabled( "%s%s  %s", g.m_Path.empty() ? "(unsaved)" : g.m_Path.string().c_str(), g.m_Modified ? " *" : "", g.m_Status.c_str() );

    ImGui::SameLine();
    ImGui::Checkbox( "Preview", &ioPreview );

    DrawCanvas();
    ImGui::SameLine();
    DrawSide();
    ImGui::End();
}
