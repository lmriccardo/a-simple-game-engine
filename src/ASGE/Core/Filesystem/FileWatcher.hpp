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
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Strings.hpp>

#include "FileData.hpp"
#include "FileMetadata.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX // Prevent windows.h from defining min()/max() macros
#endif
#include <windows.h>
#define INVALID_FD INVALID_HANDLE_VALUE
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#else
#include <sys/inotify.h>
#include <sys/epoll.h>
#include <cstring>
#define INVALID_FD (-1)
#endif

namespace asge::filesystem
{

/**
 * @brief Bit flags for the kinds of file events a watch can report.
 *
 * Combine with `|` to build a mask for AddWatch; a reported FileEvent always
 * carries exactly one of Created, Deleted, Modified or Moved.
 */
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

/**
 * @brief Bitwise OR of two event masks.
 */
constexpr FEventType operator|( FEventType in1, FEventType in2 ) noexcept;

/**
 * @brief Bitwise AND of two event masks; FEventType::None means no overlap.
 */
constexpr FEventType operator&( FEventType in1, FEventType in2 ) noexcept;

/**
 * @brief Bitwise complement of an event mask.
 */
constexpr FEventType operator~( FEventType in1 ) noexcept;

/**
 * @brief Adds the flags of in2 to in1 and returns in1.
 */
FEventType& operator|=( FEventType& in1, FEventType in2 ) noexcept;

/**
 * @brief Keeps only the flags shared with in2 and returns in1.
 */
FEventType& operator&=( FEventType& in1, FEventType in2 ) noexcept;

/* ---------------------- UTILITY OPERATION FOR PRINTING OUT -------------------- */

/**
 * @brief Prints the names of the flags set in inMask, for logging.
 */
std::ostream& operator<<( std::ostream& inOss, FEventType inMask ) noexcept;

#ifdef _WIN32
using native_event_t = DWORD;
using handle_t = HANDLE;
#else
using native_event_t = std::uint32_t;
using handle_t = std::int32_t;
#endif

/**
 * @brief One file system change reported by a FileWatcher.
 *
 * Carries what happened, to which path, and a snapshot of the file's
 * metadata taken when the event was built.
 */
struct FileEvent
{
    Path            s_Path;      // The absolute path of the file
    FEventType      s_Event;     // The event regarding that file
    time::Timestamp s_Timestamp; // When the event was detected
    Path            s_OldPath;   // Populated only for FEventType::Moved
    FileSize        s_FileSize;  // Size in bytes at event time
    FileType        s_FileType;  // Regular, directory, symlink, ...
    native_event_t  s_RawEvent;  // Native event type from OS

    /**
     * @brief Checks if the current file is a directory.
     */
    bool IsDir() const noexcept;

    /**
     * @brief Checks if the current event matches the input event type.
     */
    bool Is( FEventType inType ) const noexcept;

    /**
     * @brief Builds an event for inFullPath and fills in its metadata.
     *
     * The timestamp is taken now. Type and size are read from disk unless the
     * event is Deleted, since the file is gone by then. inOldPath is only
     * meaningful for Moved events.
     */
    static FileEvent CreateFileEvent(
        Path const& inFullPath,
        FEventType inEvenType,
        native_event_t inRawAction,
        Path const& inOldPath = ""
    ) noexcept;
};

/**
 * @brief Prints a readable one-line description of inEvent, for logging.
 */
std::ostream& operator<<(std::ostream& inOss, const FileEvent& inEvent);

/**
 * @brief Optional interface for objects that react to each kind of file event.
 */
class IFileEventHandler
{
public:
    /**
     * @brief Virtual destructor so derived types are destroyed through the interface.
     */
    virtual ~IFileEventHandler() = default;

    /**
     * @brief Called when a file or directory was created.
     */
    virtual void OnFileCreated( FileEvent const& inEvent ) = 0;

    /**
     * @brief Called when a file or directory was deleted.
     */
    virtual void OnFileDeleted( FileEvent const& inEvent ) = 0;

    /**
     * @brief Called when a file was modified.
     */
    virtual void OnFileModified( FileEvent const& inEvent ) = 0;

    /**
     * @brief Called when a file or directory was moved or renamed.
     */
    virtual void OnFileMoved( FileEvent const& inEvent ) = 0;
};

/**
 * @brief Hash functor so Path can key an unordered_map.
 */
struct PathHash
{
    /**
     * @brief Hashes p with std::filesystem::hash_value.
     */
    inline std::size_t operator()(const Path& p) const noexcept 
    {
        return std::filesystem::hash_value(p);
    }
};

using WatcherHandler = signals::Connection<FileEvent const&>;

namespace _internal
{

using callback_type = std::function<void(FileEvent const&)>;
using callback_type_cref = callback_type const&;

/**
 * @brief Wraps inCallback so it only runs for events whose type is in inMask.
 */
callback_type CreateCallback( callback_type_cref inCallback, FEventType inMask );

};

#ifdef _WIN32
namespace _win32
{

struct alignas(64) WatchedDir
{
    handle_t             s_hDir;          // Directory handle opened for change notifications
    OVERLAPPED           s_Overlapped;    // Overlapped I/O state of the pending read
    std::wstring_view    s_PathView;      // View of the watched path (key of the owning map)
    std::atomic_bool     s_Active{true};  // False once the directory is being unregistered
    std::atomic_uint32_t s_RefCount{0};   // Number of watches registered on this directory
    alignas(4) DWORD     s_Buffer[1024];  // Receives FILE_NOTIFY_INFORMATION records
};

/**
 * @brief Windows file watcher: ReadDirectoryChangesW driven through an I/O completion port.
 *
 * A Thread that waits on the port and emits a FileEvent signal for each change
 * in a watched directory. Watches are not recursive. Several watches on the
 * same directory share one OS handle.
 */
class FileWatcher : public concurrent::Thread
{
public:
    using path_type = Path;
    using mask_type = FEventType;

private:
    using watched_pointer = std::unique_ptr<WatchedDir>;

    static constexpr DWORD MILLISECONDS_TO_WAIT = 300;

    signals::Signal<FileEvent const&> m_OnEvent;                                 // Emits every detected change
    handle_t m_hIOCP; // Windows I/O Completion Port handle
    std::unordered_map<path_type, watched_pointer, PathHash> m_WatchedDirs;      // Watched directories by path
    std::mutex m_DirectoriesMutex;                                               // Guards m_WatchedDirs
    Path m_PendingOldPath;                                                       // Old name of a rename awaiting its new name

    /**
     * @brief (Re)arms the asynchronous directory read on inWdirPtr.
     *
     * Must be called again after each completion, or no further events arrive.
     */
    static bool RegisterDirChanges( WatchedDir* inWdirPtr );

    /**
     * @brief Starts watching a directory through the completion port.
     *
     * Reuses the existing entry (bumping its ref count) if the directory is
     * already watched. Returns an error Result if the handle cannot be opened.
     */
    Result<WatchedDir*> RegisterPathWithIOCP( path_type const& inPath );

    /**
     * @brief Drops one watch on inPath, closing the directory at the last one.
     */
    void UnregisterPathWithIOCP( path_type const& inPath );

    /**
     * @brief Turns the notification records in a directory's buffer into FileEvent signals.
     *
     * Pairs the old-name and new-name records of a rename into one Moved event.
     */
    void ProcessBuffer( WatchedDir* inEntry, DWORD inBytesTransferred );

    /**
     * @brief One loop pass: waits up to 300 ms for a completion and handles it.
     */
    void Run( concurrent::context_pointer& inCtx ) override;

public:
    /**
     * @brief Creates a watcher thread under the Background() context and starts it.
     */
    FileWatcher();

    /**
     * @brief Creates a watcher thread under inCtx and starts it.
     *
     * Cancelling inCtx stops the watcher.
     */
    FileWatcher(concurrent::context_pointer inCtx);

    /**
     * @brief Stops the thread and closes every watched directory.
     */
    ~FileWatcher();

    /**
     * @brief Watches a file or directory and calls inCallback for matching events.
     *
     * inPath must exist. A directory is watched for changes to its direct
     * entries (not recursively); for a file the parent directory is watched
     * and events are filtered to that path. Only events whose type is in
     * inMask reach inCallback, which runs on the watcher thread. Returns the
     * connection to Disconnect() to stop it, or an error Result.
     */
    Result<WatcherHandler> AddWatch( path_type inPath,
        _internal::callback_type_cref inCallback,
        mask_type inMask = mask_type::All
    );
};

}
using FileWatcher = _win32::FileWatcher;
#elif defined(__APPLE__)
namespace _apple
{

/**
 * @brief macOS file watcher built on FSEvents streams.
 *
 * A Thread owning one FSEvents stream per watched directory and emitting a
 * FileEvent signal for each reported change.
 */
class FileWatcher : public concurrent::Thread
{
public:
    using path_type = Path;
    using mask_type = FEventType;

private:
    struct FSEventEntry
    {
        FSEventStreamRef s_Stream; // The FSEvents stream of the directory
        dispatch_queue_t s_Queue;  // Queue the stream delivers on
    };

    mutable std::mutex m_WatchesMtx;                                          // Guards m_WatchedStreams
    std::unordered_map<path_type, FSEventEntry, PathHash> m_WatchedStreams;   // Streams by watched directory

    signals::Signal<FileEvent const&> m_OnEvent;                              // Emits every detected change

    /**
     * @brief Translates FSEvents flags into the matching FEventType.
     */
    static FEventType MapEvent( FSEventStreamEventFlags inFlags ) noexcept;

    /**
     * @brief Creates and starts an FSEvents stream for a directory.
     *
     * Does nothing successfully if the directory is already watched.
     */
    BoolResult RegisterNewWatch( path_type inPath ) noexcept;

    /**
     * @brief C callback FSEvents invokes; converts the batch into FileEvent signals.
     *
     * inInfo is the owning FileWatcher.
     */
    static void OnFSEventStream(
        ConstFSEventStreamRef inStream,
        void* inInfo,
        std::size_t inNumEvents,
        void* inEventPaths,
        FSEventStreamEventFlags const* inEventFlags,
        FSEventStreamEventId const* inEventIds
    );

    /**
     * @brief One loop pass; FSEvents delivers on its own queue, so this just idles.
     */
    void Run( concurrent::context_pointer& inCtx ) override;

public:
    /**
     * @brief Creates a watcher thread under the Background() context and starts it.
     */
    FileWatcher();

    /**
     * @brief Creates a watcher thread under inCtx and starts it.
     */
    FileWatcher(concurrent::context_pointer inCtx);

    /**
     * @brief Stops the thread and tears down every stream.
     */
    ~FileWatcher();

    /**
     * @brief Watches a file or directory and calls inCallback for matching events.
     *
     * Same contract as the other platforms: inPath must exist, a file is
     * watched through its parent directory with events filtered to it, and
     * only types in inMask reach inCallback.
     */
    Result<WatcherHandler> AddWatch( path_type inPath,
        _internal::callback_type_cref inCallback,
        mask_type inMask = mask_type::All
    );
};

}
using FileWatcher = _apple::FileWatcher;
#else
namespace _linux
{

/**
 * @brief Linux file watcher: inotify instances polled through epoll.
 *
 * A Thread that waits on an epoll set for inotify events and emits a
 * FileEvent signal for each change in a watched directory. Watches are not
 * recursive.
 */
class FileWatcher : public concurrent::Thread
{
public:
    using path_type = Path;
    using mask_type = FEventType;

private:
    static constexpr int MAX_EPOLL_EVENTS = 16;       // Events fetched per epoll_wait
    static constexpr int MILLISECONDS_TO_WAIT = 300;  // epoll_wait timeout, so cancellation is noticed

    handle_t m_InotifyHandle;                                   // The inotify file descriptor
    handle_t m_EpollHandle;                                     // The epoll file descriptor
    mutable std::mutex m_WatchesMtx;                            // Guards m_WatchFds
    bool m_InotifyValid{false};                                 // True once inotify was created
    bool m_EpollValid{false};                                   // True once epoll was created

    signals::Signal<FileEvent const&> m_OnEvent;                // Emits every detected change
    std::unordered_map<path_type, handle_t, PathHash> m_WatchFds;  // Watch descriptor per watched directory
    std::unordered_map<std::uint32_t, std::string> m_PendingMoves; // Old names of renames by inotify cookie

    /**
     * @brief Checks whether all handles are valid or not.
     */
    bool isValid() const noexcept;

    /**
     * @brief Adds an inotify watch for a directory.
     *
     * Does nothing successfully if the directory is already watched.
     */
    BoolResult RegisterNewWatch( path_type inPath ) noexcept;

    /**
     * @brief Reads every pending inotify record and hands each to ConsumeInotifyEvent.
     */
    void ReadInotifyEvents();

    /**
     * @brief Converts one inotify record into a FileEvent signal.
     *
     * Pairs the moved-from and moved-to records of a rename (matched by cookie)
     * into one Moved event.
     */
    void ConsumeInotifyEvent( inotify_event const* inEvent );

    /**
     * @brief Finds the watched directory a watch descriptor belongs to.
     *
     * Returns an error Result (FileWatcherError::InvalidWatchDescriptor) if unknown.
     */
    Result<path_type> GetParentPathFromWd( handle_t inWd ) const noexcept;

    /**
     * @brief One loop pass: waits up to 300 ms on epoll and reads ready events.
     */
    void Run( concurrent::context_pointer& inCtx ) override;
public:
    /**
     * @brief Creates a watcher thread under the Background() context and starts it.
     */
    FileWatcher();

    /**
     * @brief Creates a watcher thread under inCtx and starts it.
     */
    FileWatcher(concurrent::context_pointer inCtx);

    /**
     * @brief Stops the thread and closes the inotify and epoll descriptors.
     */
    ~FileWatcher();

    /**
     * @brief Watches a file or directory and calls inCallback for matching events.
     *
     * Same contract as the other platforms: inPath must exist, a file is
     * watched through its parent directory with events filtered to it, and
     * only types in inMask reach inCallback.
     */
    Result<WatcherHandler> AddWatch( path_type inPath,
        _internal::callback_type_cref inCallback,
        mask_type inMask = mask_type::All );
};

}
using FileWatcher = _linux::FileWatcher;
#endif

}