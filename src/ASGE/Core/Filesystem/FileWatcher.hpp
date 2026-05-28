#pragma once

#include <cstdint>
#include <string>
#include <ASGE/Core/Patterns/Signal.hpp>
#include <ASGE/Core/Concurrent/Thread.hpp>

#ifdef _WIN32
#include <windows.h>
#define INVALID_FD INVALID_HANDLE_VALUE
#else
#include <sys/inotify.h>
#define INVALID_FD (-1)
#endif

namespace asge::filesystem
{

#ifdef _WIN32
using handle_t = HANDLE;
#else
using handle_t = std::int32_t;
#endif

class FileWatcher : public concurrent::Thread
{
    void Run( concurrent::context_pointer& inCtx ) override;
public:
    FileWatcher( std::string const& inPath );
};

}