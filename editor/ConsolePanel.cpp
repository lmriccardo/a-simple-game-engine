#include "ConsolePanel.hpp"
#include "FileDialog.hpp"

#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Filesystem/FileIO.hpp>

#include <imgui.h>

#include <deque>
#include <mutex>
#include <sstream>
#include <vector>

namespace
{
using asge::logger::LogLevel;
using asge::logger::LogRecord;

// Logger's OnLog signal can fire from any thread (e.g. the editor's own
// native file-dialog callback logs from a non-main thread, see main.cpp's
// OnFileDialogResult), so appends are mutex-guarded; DrawConsolePanel takes
// a snapshot under the same lock rather than holding it while drawing.
constexpr std::size_t kMaxEntries = 500;

std::mutex g_Mutex;
std::deque<LogRecord> g_Entries;

FileDialogResult g_SaveLogDialogResult;
constexpr SDL_DialogFileFilter kLogFileFilters[]{ { "Log (*.log)", "log" } };

void OnLog( LogRecord const& inRecord ) noexcept
{
    std::lock_guard const lock( g_Mutex );
    g_Entries.push_back( inRecord );
    if ( g_Entries.size() > kMaxEntries ) g_Entries.pop_front();
}

ImVec4 ColorFor( LogLevel inLevel ) noexcept
{
    switch ( inLevel )
    {
    case LogLevel::Debug:   return ImVec4( 0.6f, 0.6f, 0.6f, 1.0f );
    case LogLevel::Warning: return ImVec4( 1.0f, 0.8f, 0.2f, 1.0f );
    case LogLevel::Error:   return ImVec4( 1.0f, 0.35f, 0.35f, 1.0f );
    case LogLevel::Info:    return ImVec4( 0.85f, 0.85f, 0.85f, 1.0f );
    }
    return ImVec4( 0.85f, 0.85f, 0.85f, 1.0f );
}

char const* LabelFor( LogLevel inLevel ) noexcept
{
    switch ( inLevel )
    {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Warning: return "WARNING";
    case LogLevel::Error:   return "ERROR";
    case LogLevel::Info:    return "INFO";
    }
    return "INFO";
}

// Single source of truth for one entry's text -- used both for the panel's
// on-screen rows and for "Save Log"'s file content, so the two never drift
// out of sync with each other.
std::string FormatLine( LogRecord const& inRecord ) noexcept
{
    std::ostringstream oss;
    oss << "[" << LabelFor( inRecord.m_Level ) << "]"
        << "[" << asge::time::FormatTimestamp( inRecord.m_Timestamp, "%H:%M:%S" ) << "]"
        << "[" << inRecord.m_Function << "] "
        << inRecord.m_Message;
    return oss.str();
}

// A small filled shape (circle for errors, triangle for warnings) plus a
// count, e.g. "(x) 3" -- ImGui's title bar can't host a custom draw call, so
// this lives in the toolbar row instead of beside the window title itself.
void DrawCountBadge( ImU32 inColor, bool inTriangle, std::size_t inCount ) noexcept
{
    float const r = 6.0f;
    ImVec2 const p = ImGui::GetCursorScreenPos();
    float const lineH = ImGui::GetTextLineHeight();
    ImVec2 const center( p.x + r, p.y + lineH * 0.65f );

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if ( inTriangle )
    {
        drawList->AddTriangleFilled(
            ImVec2( center.x, center.y - r ),
            ImVec2( center.x - r, center.y + r * 0.75f ),
            ImVec2( center.x + r, center.y + r * 0.75f ),
            inColor );
    }
    else
    {
        drawList->AddCircleFilled( center, r, inColor );
    }

    ImGui::Dummy( ImVec2( r * 2.0f + 6.0f, lineH ) );
    ImGui::SameLine();
    ImGui::Text( "%zu", inCount );
}
}

void InitConsolePanel() noexcept
{
    // Kept only to prove Connect succeeded (unused otherwise) -- the
    // connection is meant to live for the process's whole lifetime, so it's
    // never explicitly disconnected.
    [[maybe_unused]] static auto const connection = asge::logger::Logger::Instance().OnLogConnect( OnLog );
}

void DrawConsolePanel( SDL_Window* inWindow ) noexcept
{
    std::vector<LogRecord> snapshot;
    {
        std::lock_guard const lock( g_Mutex );
        snapshot.assign( g_Entries.begin(), g_Entries.end() );
    }

    std::size_t errorCount = 0;
    std::size_t warningCount = 0;
    for ( auto const& record : snapshot )
    {
        if ( record.m_Level == LogLevel::Error ) ++errorCount;
        else if ( record.m_Level == LogLevel::Warning ) ++warningCount;
    }

    // Save Log's result is drained before the window's own controls, same
    // convention as main.cpp's Save/Open drains -- the dialog callback never
    // touches g_Entries/the filesystem itself, only queues a path here.
    {
        std::string chosenPath;
        if ( DrainFileDialogResult( g_SaveLogDialogResult, chosenPath ) )
        {
            asge::filesystem::Path path = chosenPath;
            if ( path.extension().empty() ) path += ".log"; // native dialogs don't all enforce the filter's extension

            std::string content;
            {
                std::lock_guard const lock( g_Mutex );
                for ( auto const& record : g_Entries ) content += FormatLine( record ) + "\n";
            }

            auto const writeResult = asge::filesystem::WriteText( path, content );
            if ( !writeResult ) writeResult.LogError();
            else LOG_INFO( "Log saved to ", path.string() );
        }
    }

    // Anchored flush to the bottom and both side edges of the whole editor
    // window -- forced every frame (ImGuiCond_Always) since DisplaySize can
    // change (Phase 7's planned resizable window) and the panel must stay
    // flush regardless. Height alone stays user-draggable: the size
    // constraint's min/max width are equal (locking that axis) while height
    // ranges freely, and g_Height persists whatever the user last dragged it
    // to across frames -- without that, next frame's forced position/size
    // would immediately overwrite a drag before it could ever show.
    static float g_Height = 200.0f;
    float const width = ImGui::GetIO().DisplaySize.x;
    float const y = ImGui::GetIO().DisplaySize.y - g_Height;

    ImGui::SetNextWindowPos( ImVec2( 0.0f, y ), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2( width, g_Height ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSizeConstraints(
        ImVec2( width, 80.0f ), ImVec2( width, ImGui::GetIO().DisplaySize.y - 60.0f ) );

    ImGui::Begin( "Log", nullptr, ImGuiWindowFlags_NoMove );
    g_Height = ImGui::GetWindowSize().y; // picks up this frame's drag-resize (if any) for next frame's position

    if ( ImGui::Button( "Clear" ) )
    {
        std::lock_guard const lock( g_Mutex );
        g_Entries.clear();
        snapshot.clear();
    }

    if ( errorCount > 0 )
    {
        ImGui::SameLine();
        DrawCountBadge( IM_COL32( 230, 60, 60, 255 ), false, errorCount );
    }
    if ( warningCount > 0 )
    {
        ImGui::SameLine();
        DrawCountBadge( IM_COL32( 230, 180, 40, 255 ), true, warningCount );
    }

    // Computed from the actual button size (not a guessed constant) so it
    // stays flush against the right edge of the content region regardless of
    // how wide that region is -- including after a resize, once Phase 7
    // makes the editor window itself resizable (this panel's own width
    // already tracks ImGui::GetIO().DisplaySize.x every frame; the button's
    // position needs to track that same live width, not a value baked in
    // for one particular window size).
    char const* const kSaveLogLabel = "Save Log";
    float const saveLogWidth = ImGui::CalcTextSize( kSaveLogLabel ).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SameLine( ImGui::GetWindowWidth() - saveLogWidth - ImGui::GetStyle().WindowPadding.x );
    if ( ImGui::Button( kSaveLogLabel ) )
    {
        SDL_ShowSaveFileDialog(
            OnFileDialogResult, &g_SaveLogDialogResult, inWindow, kLogFileFilters, 1, nullptr );
    }

    ImGui::Separator();

    // ImGuiWindowFlags_HorizontalScrollbar -- Text/TextColored don't wrap on
    // their own, and without it a line longer than the panel's width was
    // simply clipped with no way to read the rest of it.
    ImGui::BeginChild( "LogScroll", ImVec2( 0.0f, 0.0f ), false, ImGuiWindowFlags_HorizontalScrollbar );
    for ( auto const& record : snapshot )
    {
        ImGui::TextColored( ColorFor( record.m_Level ), "%s", FormatLine( record ).c_str() );
    }
    // Auto-scroll only while already at the bottom, so scrolling up to read
    // an earlier entry isn't yanked back down by the next log line.
    if ( ImGui::GetScrollY() >= ImGui::GetScrollMaxY() ) ImGui::SetScrollHereY( 1.0f );
    ImGui::EndChild();
    ImGui::End();
}
