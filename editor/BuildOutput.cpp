#include "BuildOutput.hpp"

#include <deque>
#include <mutex>

namespace
{

constexpr std::size_t kMaxLines = 5000;

std::mutex g_Mutex;
std::deque<BuildLine> g_Lines;

}

void AppendBuildOutput( BuildLevel inLevel, std::string inText )
{
    std::lock_guard const lock( g_Mutex );
    g_Lines.push_back( { inLevel, std::move( inText ) } );
    if ( g_Lines.size() > kMaxLines ) g_Lines.pop_front();
}

std::vector<BuildLine> SnapshotBuildOutput()
{
    std::lock_guard const lock( g_Mutex );
    return { g_Lines.begin(), g_Lines.end() };
}

void ClearBuildOutput()
{
    std::lock_guard const lock( g_Mutex );
    g_Lines.clear();
}
