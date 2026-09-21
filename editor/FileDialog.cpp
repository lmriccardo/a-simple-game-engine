#include "FileDialog.hpp"

#include <ASGE/Core/Logger/Logger.hpp>

void OnFileDialogResult( void* inUserdata, char const* const* inFileList, int ) noexcept
{
    auto* result = static_cast<FileDialogResult*>( inUserdata );
    std::lock_guard const lock( result->m_Mutex );

    if ( inFileList == nullptr )
    {
        LOG_ERROR( "File dialog failed: ", SDL_GetError() );
        result->m_Accepted = false;
    }
    else if ( inFileList[0] == nullptr )
    {
        result->m_Accepted = false; // user canceled
    }
    else
    {
        result->m_Accepted = true;
        result->m_Path = inFileList[0];
    }
    result->m_Ready = true;
}

bool DrainFileDialogResult( FileDialogResult& inResult, std::string& outPath ) noexcept
{
    bool ready = false;
    bool accepted = false;
    std::string path;
    {
        std::lock_guard const lock( inResult.m_Mutex );
        ready = inResult.m_Ready;
        accepted = inResult.m_Accepted;
        path = inResult.m_Path;
        inResult.m_Ready = false;
    }

    if ( !ready || !accepted ) return false;

    outPath = std::move( path );
    return true;
}

std::string DialogDefaultLocation( std::filesystem::path const& inDir ) noexcept
{
    if ( inDir.empty() ) return {};
    return ( inDir / "" ).string(); // forces a trailing separator -- see this function's own doc comment
}
