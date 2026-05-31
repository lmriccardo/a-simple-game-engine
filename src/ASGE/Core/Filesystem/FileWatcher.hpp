#pragma once

#include <filesystem>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <type_traits>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <iomanip>

#include <ASGE/Core/Time/TimeUtils.hpp>
#include <ASGE/Core/Logger/Logger.hpp>
#include <ASGE/Core/Concurrent/Thread.hpp>
#include <ASGE/Core/Patterns/Signal.hpp>

#ifdef _WIN32
#include <windows.h>
#define INVALID_FD INVALID_HANDLE_VALUE
#else
#define INVALID_FD (-1)
#endif

namespace asge::filesystem
{

enum class FEventType : std::uint8_t
{
    None      = 0,      // Unknown Event
    Created   = 1 << 0, // File was created in watched directory
    Deleted   = 1 << 1, // File was deleted from watched directory
    Modified  = 1 << 2, // File was modified ( for example writing into it )
    Moved     = 1 << 3, // File was moved from to a folder

    All = Created | Deleted | Modified | Moved // All events at one
};

/* ---------------------- UTILITY OPERATIONS FOR MASKING -------------------- */

constexpr FEventType operator|( FEventType in1, FEventType in2 ) noexcept;
constexpr FEventType operator&( FEventType in1, FEventType in2 ) noexcept;
constexpr FEventType operator~( FEventType in1 ) noexcept;

FEventType& operator|=( FEventType& in1, FEventType in2 ) noexcept;
FEventType& operator&=( FEventType& in1, FEventType in2 ) noexcept;

/* ---------------------- UTILITY OPERATION FOR PRINTING OUT -------------------- */

std::ostream& operator<<( std::ostream& inOss, FEventType inMask ) noexcept;

#ifdef _WIN32
using native_event_t = DWORD;
using handle_t = HANDLE;
#else
using native_event_t = std::uint32_t;
using handle_t = std::int32_t;
#endif

struct FileEvent
{
    std::filesystem::path      s_Path;      // The absolute path of the file
    FEventType                 s_Event;     // The event regarding that file
    time::Timestamp            s_Timestamp; // When the event was detected
    std::filesystem::path      s_OldPath;   // Populated only for FEventType::Moved
    std::uintmax_t             s_FileSize;  // Size in bytes at event time
    std::filesystem::file_type s_FileType;  // Regular, directory, symlink, ...
    native_event_t             s_RawEvent;  // Native event type from OS

    /* Checks if the current file is a directory */
    bool IsDir() const noexcept;

    /* Checks if the current event matches the input event type */
    bool Is( FEventType inType ) const noexcept;

    static FileEvent CreateFileEvent(
        std::filesystem::path const& inFullPath,
        FEventType inEvenType,
        native_event_t inRawAction,
        std::filesystem::path const& inOldPath = ""
    ) noexcept;
};

std::ostream& operator<<(std::ostream& inOss, const FileEvent& inEvent);

class IFileEventHandler
{
public:
    virtual ~IFileEventHandler() = default;
    virtual void OnFileCreated( FileEvent const& inEvent ) = 0;
    virtual void OnFileDeleted( FileEvent const& inEvent ) = 0;
    virtual void OnFileModified( FileEvent const& inEvent ) = 0;
    virtual void OnFileMoved( FileEvent const& inEvent ) = 0;
};

struct PathHash 
{
    inline std::size_t operator()(const std::filesystem::path& p) const noexcept 
    {
        return std::filesystem::hash_value(p);
    }
};

#ifdef _WIN32
namespace _win32
{

struct alignas(64) WatchedDir
{
    handle_t             s_hDir;
    OVERLAPPED           s_Overlapped;
    std::wstring_view    s_PathView;
    std::atomic_bool     s_Active{true};
    std::atomic_uint32_t s_RefCount{0};
    alignas(4) DWORD     s_Buffer[1024];
};

class FileWatcher : public concurrent::Thread
{
public:
    using callback_type = std::function<void(FileEvent const&)>;
    using callback_type_cref = callback_type const&;
    using path_type = std::filesystem::path;
    using watcher_handle_type = signals::Connection<FileEvent const&>;
    using mask_type = FEventType;

private:
    using watched_pointer = std::unique_ptr<WatchedDir>;

    static constexpr DWORD MILLISECONDS_TO_WAIT = 300;

    signals::Signal<FileEvent const&> m_OnEvent;
    handle_t m_hIOCP; // Windows I/O Completion Port handle
    std::unordered_map<path_type, watched_pointer, PathHash> m_WatchedDirs;
    std::mutex m_DirectoriesMutex;
    std::filesystem::path m_PendingOldPath;

    static bool RegisterDirChanges( WatchedDir* inWdirPtr );
    static callback_type CreateCallback( callback_type_cref inCallback, mask_type inMask );

    WatchedDir* RegisterPathWithIOCP( path_type const& inPath );
    void UnregisterPathWithIOCP( path_type const& inPath );
    void ProcessBuffer( WatchedDir* inEntry, DWORD inBytesTransferred );

    void Run( concurrent::context_pointer& inCtx ) override;

public:
    FileWatcher();
    ~FileWatcher();

    watcher_handle_type AddWatch( path_type inPath, 
        callback_type_cref inCallback, 
        mask_type inMask = mask_type::All );
};

}
#else
namespace _unix
{
    
}
#endif

}