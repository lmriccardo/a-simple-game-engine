#include "FileWatcher.hpp"

using namespace asge::filesystem;

constexpr FEventType asge::filesystem::operator|(FEventType in1, FEventType in2) noexcept
{
    using T = std::underlying_type_t<FEventType>;
    return static_cast<FEventType>( static_cast<T>(in1) | static_cast<T>(in2) );
}

constexpr FEventType asge::filesystem::operator&(FEventType in1, FEventType in2) noexcept
{
    using T = std::underlying_type_t<FEventType>;
    return static_cast<FEventType>( static_cast<T>(in1) & static_cast<T>(in2) );
}

constexpr FEventType asge::filesystem::operator~(FEventType in1) noexcept
{
    using T = std::underlying_type_t<FEventType>;
    return static_cast<FEventType>( ~static_cast<T>(in1) );
}

FEventType &asge::filesystem::operator|=(FEventType &in1, FEventType in2) noexcept
{
    in1 = in1 | in2;
    return in1;
}

FEventType &asge::filesystem::operator&=(FEventType &in1, FEventType in2) noexcept
{
    in1 = in1 & in2;
    return in1;
}

std::ostream &asge::filesystem::operator<<(std::ostream &inOss, FEventType inMask) noexcept
{
    // If no flags are set, return early
    if (inMask == FEventType::None) return inOss << "None";

    // A static compile-time map of flags to strings
    struct FlagMapping { FEventType flag; const char* name; };
    static constexpr FlagMapping mappings[] = {
        { FEventType::Created,  "Created" },
        { FEventType::Deleted,  "Deleted" },
        { FEventType::Modified, "Modified" },
        { FEventType::Moved,    "Moved" }
    };

    bool first{ true };
    for (const auto& [flag, name] : mappings)
    {
        if ((inMask & flag) != FEventType::None)
        {
            if (!first)  inOss << ", ";
            inOss << name;
            first = false;
        }
    }

    return inOss;
}

std::ostream &asge::filesystem::operator<<(std::ostream &inOss, const FileEvent &inEvent)
{
    // Format timestamp nicely (e.g., converting steady_clock to a relative duration or string)
    // For simplicity, we can show milliseconds elapsed since epoch or program start
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        inEvent.s_Timestamp.time_since_epoch()
    ).count();

    inOss << "FileEvent [\n"
          << "  Timestamp: " << ms << "ms\n"
          << "  Event:     " << inEvent.s_Event << "\n" // Uses your custom FEventType operator<<
          << "  Path:      " << inEvent.s_Path.string() << "\n";

    // Only print OldPath if it's a Moved event and has data
    if (inEvent.Is(FEventType::Moved) && !inEvent.s_OldPath.empty())
    {
        inOss << "  From Path: " << inEvent.s_OldPath.string() << "\n";
    }

    // Print File Type information
    inOss << "  Type:      " << (inEvent.IsDir() ? "Directory" : "File") 
          << " (" << static_cast<int>(inEvent.s_FileType) << ")\n";

    // Only print meaningful sizes (Skip for directories or deletions)
    if (!inEvent.IsDir() && inEvent.s_Event != FEventType::Deleted)
    {
        inOss << "  Size:      " << inEvent.s_FileSize << " bytes\n";
    }

    inOss << "  Raw OS ID: 0x" << std::hex << inEvent.s_RawEvent << std::dec << "\n"
          << "]";

    return inOss;
}

bool asge::filesystem::FileEvent::IsDir() const noexcept
{
    return s_FileType == std::filesystem::file_type::directory;
}

bool asge::filesystem::FileEvent::Is(FEventType inType) const noexcept
{
    return s_Event == inType;
}

FileEvent asge::filesystem::FileEvent::CreateFileEvent(
    Path const &inFullPath, FEventType inEvenType, 
    native_event_t inRawAction, Path const &inOldPath
) noexcept {
    FileEvent event;
    event.s_Path      = inFullPath;
    event.s_Event     = inEvenType;
    event.s_RawEvent  = inRawAction;
    event.s_OldPath   = inOldPath;
    event.s_Timestamp = time::Now();

    // Gather file system metadata safely
    std::error_code ec;
    if (inEvenType != FEventType::Deleted)
    {
        // 1. Get File Type (Regular, Directory, Symlink, etc.)
        auto status = std::filesystem::status(inFullPath, ec);
        if (!ec) {
            event.s_FileType = status.type();
        }

        // 2. Get File Size (Only for regular files, directories will return errors or 0)
        if (event.s_FileType == std::filesystem::file_type::regular) {
            auto size = std::filesystem::file_size(inFullPath, ec);
            if (!ec) {
                event.s_FileSize = size;
            }
        }
    }
    else
    {
        // If it was deleted, we can't look it up on disk. 
        // We assume it was a regular file, or leave it as unknown, size 0.
        event.s_FileType = std::filesystem::file_type::not_found;
        event.s_FileSize = 0;
    }

    return event;
}

asge::filesystem::_internal::callback_type 
asge::filesystem::_internal::CreateCallback(callback_type_cref inCallback, FEventType inMask)
{
    return [ inCallback, inMask ]( FileEvent const& e )
    {
        if ( ( e.s_Event & inMask ) != FEventType::None )
        {
            inCallback( e );
        }
    };
}

#ifdef _WIN32
bool asge::filesystem::_win32::FileWatcher::RegisterDirChanges(WatchedDir* inWdirPtr)
{
    DWORD bytesReturned = 0;
    BOOL  successRead = ReadDirectoryChangesW(
        inWdirPtr->s_hDir,
        inWdirPtr->s_Buffer,
        sizeof( inWdirPtr->s_Buffer ),
        FALSE, // True for recursive watching
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
        &bytesReturned,
        &inWdirPtr->s_Overlapped,
        nullptr
    );

    if (!successRead)
    {
        return (GetLastError() == ERROR_IO_PENDING);
    }

    return true; // Succeeded immediately
}

asge::Result<asge::filesystem::_win32::WatchedDir*>
asge::filesystem::_win32::FileWatcher::RegisterPathWithIOCP(path_type const &inPath)
{
    std::lock_guard<std::mutex> lock( m_DirectoriesMutex );
    
    // Case 1: Path is already registered! Just increment the reference count.
    auto it = m_WatchedDirs.find( inPath );
    if ( it != m_WatchedDirs.end() )
    {
        it->second->s_RefCount++;
        return Result<WatchedDir*>::Ok(it->second.get());
    }

    // Case 2: Brand new path. Open the directory handle and bind to IOCP.
    handle_t hDirectory = CreateFileW(
        inPath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    // Handle error (throw exception or return nullptr)
    if ( hDirectory == INVALID_HANDLE_VALUE )
    {
        auto const ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        return Result<WatchedDir*>::Err(ec, "Failed to open directory watching" 
            + str::ToUTF8(inPath.u8string()));
    }

    // Try emplacing a new entry so we have a persistent memory address
    watched_pointer watchedDir = std::make_unique<WatchedDir>();
    auto [ insertedIt, success ] = m_WatchedDirs.try_emplace( inPath, std::move(watchedDir) );
    if (!success)
    {
        auto const ec = make_error_code(errors::FileWatcherError::AlreadyWatched);
        return Result<WatchedDir*>::Err( ec, str::ToUTF8(inPath.u8string()) );
    }

    watched_pointer& entry = insertedIt->second;

    // Initialize all fields
    entry->s_hDir = hDirectory;
    entry->s_RefCount = 1;
    entry->s_Active = true;

    // Pass a view pointing directly to the map's persistent string key (zero allocation)
    entry->s_PathView = insertedIt->first.native();

    // Zero out the OVERLAPPED structure completely
    std::memset(&entry->s_Overlapped, 0, sizeof(OVERLAPPED));

    // Associate the file handle with our existing Completion Port.
    handle_t hIOCPCheck = CreateIoCompletionPort( 
        hDirectory, 
        m_hIOCP, 
        reinterpret_cast<ULONG_PTR>(entry.get()),
        0
    );

    if ( !RegisterDirChanges( entry.get() ) )
    {
        CloseHandle( hDirectory );
        m_WatchedDirs.erase( insertedIt );
        auto const ec = make_error_code(errors::FileWatcherError::FailedDirRegister);
        return Result<WatchedDir*>::Err( ec, str::ToUTF8(inPath.u8string()) );
    }

    return Result<WatchedDir*>::Ok(entry.get());
}

void asge::filesystem::_win32::FileWatcher::UnregisterPathWithIOCP(path_type const &inPath)
{
    std::lock_guard<std::mutex> lock(m_DirectoriesMutex);
    
    // Search the path into the map of all registered paths
    auto it = m_WatchedDirs.find( inPath );
    if ( it == m_WatchedDirs.end() ) return;

    it->second->s_RefCount.store( it->second->s_RefCount - 1, std::memory_order_relaxed );
    if ( it->second->s_RefCount.load( std::memory_order_relaxed ) == 0 )
    {
        CloseHandle( it->second->s_hDir );
        m_WatchedDirs.erase( it );
    }
}

void asge::filesystem::_win32::FileWatcher::ProcessBuffer(WatchedDir *inEntry, DWORD inBytesTransferred)
{
    // If the OS transferred 0 bytes, the buffer overflowed (too many events occurred at once)
    if ( inBytesTransferred == 0 ) return;

    // Point to the beginning of our aligned DWORD buffer
    auto* pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(inEntry->s_Buffer);
    while ( pNotify != nullptr )
    {
        // Combine the watched root directory path and the relative filename
        std::wstring_view relPathView( pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR) );
        Path fullPath = Path(inEntry->s_PathView) / relPathView;

        // Map the Win32 Action DWORD to your custom FEventType enum
        if ( pNotify->Action == FILE_ACTION_RENAMED_OLD_NAME )
        {   
            // Cache the old name for the NEXT record in the buffer (rename new name)
            m_PendingOldPath = fullPath;
        }
        else if ( pNotify->Action == FILE_ACTION_RENAMED_NEW_NAME )
        {
            // We have both names and we can fire the move event
            FileEvent event = FileEvent::CreateFileEvent(
                fullPath, FEventType::Moved, pNotify->Action, m_PendingOldPath
            );

            m_OnEvent.Emit( event );
            m_PendingOldPath.clear(); // Reset
        }
        else
        {
            // Standard Created, Deleted, or Modified event
            FEventType type = FEventType::None;
            if (pNotify->Action == FILE_ACTION_ADDED)     type = FEventType::Created;
            if (pNotify->Action == FILE_ACTION_REMOVED)   type = FEventType::Deleted;
            if (pNotify->Action == FILE_ACTION_MODIFIED)  type = FEventType::Modified;

            FileEvent event = FileEvent::CreateFileEvent(fullPath, type, pNotify->Action);
            m_OnEvent.Emit(event);
        }

        if (pNotify->NextEntryOffset != 0)
        {
            pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<BYTE*>(pNotify) + pNotify->NextEntryOffset
            );
        }
        else
        {
            pNotify = nullptr; // End of the linked list reached
        }
    }
}

void asge::filesystem::_win32::FileWatcher::Run(concurrent::context_pointer &inCtx)
{
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED lpOverlapped = nullptr;

    BOOL status = GetQueuedCompletionStatus(m_hIOCP, &bytesTransferred, 
        &completionKey, &lpOverlapped, MILLISECONDS_TO_WAIT
    );

    // If timeout happened or any other errors continue
    if ( !status || !lpOverlapped ) return;

    // Early returns if the context has been canceled
    if ( inCtx->Done() ) return;

    // Recover our WatchedDir context completely using the CompletionKey
    WatchedDir* entry = reinterpret_cast<WatchedDir*>( completionKey );
    
    // Process the events currently sitting in entry->s_Buffer and emit signals
    ProcessBuffer( entry, bytesTransferred );

    // Re-arm the engine immediately!
    if ( bool failed = !RegisterDirChanges( entry ) )
    {
        entry->s_Active.store(false, std::memory_order_relaxed);
    }
}

asge::filesystem::_win32::FileWatcher::FileWatcher()
: FileWatcher( nullptr ) {}

asge::filesystem::_win32::FileWatcher::FileWatcher(concurrent::context_pointer inCtx)
: concurrent::Thread( "FileWatcher", inCtx )
{
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    LOG_INFO("New File Watcher Created");
}

asge::filesystem::_win32::FileWatcher::~FileWatcher()
{
    Cancel();

    if (m_hIOCP != INVALID_HANDLE_VALUE)
    {
        PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr);
    }

    Join();

    for ( auto const& [_, v] : m_WatchedDirs )
    {
        CloseHandle( v->s_hDir );
    }

    if (m_hIOCP != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hIOCP);
    }
}

asge::Result<WatcherHandler> asge::filesystem::_win32::FileWatcher::AddWatch(
    path_type inPath, _internal::callback_type_cref inCallback, mask_type inMask
) {
    // Check if the input path actually exists
    if ( !meta::Exists( inPath ) )
    {
        auto const ec = std::make_error_code( std::errc::no_such_file_or_directory );
        return asge::Result<WatcherHandler>::Err( ec, str::ToUTF8( inPath.u8string() ) );
    }

    // Check if the input path is a regular file or a folder
    if ( meta::IsDirectory( inPath ) )
    {
        // First we need to tell windows to watch the directory
        auto result = RegisterPathWithIOCP( inPath );
        if ( !result )
        {
            return Result<WatcherHandler>::Err( result.Error() );
        }

        LOG_INFO("Added new watch for ", inPath, " with mask ", inMask);
        auto connector = m_OnEvent.Connect( _internal::CreateCallback( inCallback, inMask ) );
        return Result<WatcherHandler>::Ok(connector);
    }
    else
    {
        // First normalize the path, then take the parent folder and the filename
        inPath = inPath.lexically_normal();
        path_type parentPath = inPath.parent_path();
        auto result = RegisterPathWithIOCP( parentPath );
        if ( !result )
        {
            return Result<WatcherHandler>::Err( result.Error() );
        }

        // Creates a new callback that filters the event by the path name
        auto filtered = [ inCallback, inPath ]( FileEvent const& e ) {
            if ( !e.IsDir() && e.s_Path == inPath ) inCallback( e );
        };

        // Registers the callback into the signals
        LOG_INFO("Added new watch for ", inPath, " with mask ", inMask);
        auto connector = m_OnEvent.Connect( CreateCallback( filtered, inMask ) );
        return Result<WatcherHandler>::Ok(connector);
    }
}

#else

bool asge::filesystem::_linux::FileWatcher::isValid() const noexcept
{
    return m_InotifyValid && m_EpollValid;
}

asge::BoolResult asge::filesystem::_linux::FileWatcher::RegisterNewWatch(path_type inPath) noexcept
{
    // Check for file watcher validity first
    if ( !isValid() ) return BoolResult::Err( make_error_code( 
        errors::FileWatcherError::InvalidFwHandle ) );

    // Check if the path already exists
    auto path_it = m_WatchFds.find( inPath );
    if ( path_it != m_WatchFds.end() ) return BoolResult::Ok();

    // Otherwise register new watcher
    str::String u8_path = str::ToUTF8( inPath.u8string() );
    handle_t watchFd = ::inotify_add_watch( 
        m_InotifyHandle, 
        u8_path.c_str(), 
        IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO );

    if ( watchFd < 0 )
    {
        return BoolResult::Err( MakeErrorFromErrno(), u8_path );
    }

    m_WatchFds.emplace( inPath, watchFd );
    return BoolResult::Ok();
}

void asge::filesystem::_linux::FileWatcher::ReadInotifyEvents()
{
    alignas (struct inotify_event) char buff[4096];
    for (;;)
    {
        ssize_t len = ::read( m_InotifyHandle, buff, sizeof(buff) );
        if ( len < 0 )
        {
            if ( errno == EAGAIN || errno == EINTR ) break;
            Result<int>::Err( MakeErrorFromErrno() ).LogError();
            break;
        }

        ssize_t offset {0};
        while ( offset < len )
        {
            auto const* event = reinterpret_cast<inotify_event const*>( buff + offset );
            ConsumeInotifyEvent( event );
            offset += sizeof( struct inotify_event ) + event->len;
        }
    }
}

void asge::filesystem::_linux::FileWatcher::ConsumeInotifyEvent(inotify_event const *inEvent)
{
    std::uint32_t eventMask = inEvent->mask;
    
    // Check for moves operations, in particular move from appends the
    // current cookie into the vector of pending moves
    if ( eventMask & IN_MOVED_FROM != 0 )
    {
        m_PendingMoves.emplace( inEvent->cookie, inEvent->name );
        return;
    }
    
    FEventType eventType = FEventType::None;
    std::string oldPath{""};
    if ( eventMask & IN_MOVED_TO != 0 )
    {
        auto pending_it = m_PendingMoves.find(inEvent->cookie);
        if ( pending_it == m_PendingMoves.end() ) return;
        oldPath = pending_it->second;
        m_PendingMoves.erase( pending_it );
        eventType = FEventType::Moved;
    }
    else
    {
        if ( eventMask & IN_CREATE != 0 ) eventType = FEventType::Created;
        if ( eventMask & IN_MODIFY != 0 ) eventType = FEventType::Modified;
        if ( eventMask & IN_DELETE != 0 ) eventType = FEventType::Deleted;
    }

    if ( eventType == FEventType::None ) return;
    auto currFileEvent = FileEvent::CreateFileEvent(
        path_type( std::string( inEvent->name ) ),
        eventType,
        eventMask,
        oldPath
    );

    m_OnEvent.Emit( currFileEvent );
}

void asge::filesystem::_linux::FileWatcher::Run(concurrent::context_pointer &inCtx)
{
    if (!isValid())
    {
        LOG_ERROR("Unable to run an invalid file watcher");
        inCtx->Cancel( concurrent::ContextErr::Canceled );
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_WatchesMtx);
        if ( m_WatchFds.empty() ) return;
    }

    epoll_event events[ MAX_EPOLL_EVENTS ];
    int n = ::epoll_wait( m_EpollHandle, events, MAX_EPOLL_EVENTS, -1);
    if ( n < 0 )
    {
        // If an interrupt signal occurs ( close on the handle when canceling )
        if ( errno != EINTR ) Result<int>::Err( MakeErrorFromErrno() ).LogError();
        return;
    }

    for ( int ii = 0; ii < n; ++ii )
    {
        if ( events[ii].data.fd == m_InotifyHandle )
        {
            ReadInotifyEvents();
        }
    }
}

asge::filesystem::_linux::FileWatcher::FileWatcher()
: FileWatcher( nullptr )
{}

asge::filesystem::_linux::FileWatcher::FileWatcher( concurrent::context_pointer inCtx )
: concurrent::Thread( "FileWatcher", inCtx )
{
    m_InotifyHandle = ::inotify_init1( IN_NONBLOCK | IN_CLOEXEC );
    m_EpollHandle = ::epoll_create1( EPOLL_CLOEXEC );

    m_InotifyValid = m_InotifyHandle != INVALID_FD;
    m_EpollValid = m_EpollHandle != INVALID_FD;

    if ( !isValid() ) {
        LOG_ERROR("Error when creating FileWatcher: {}", std::strerror( errno ));
    } else {
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = m_InotifyHandle;

        if ( ::epoll_ctl( m_EpollHandle, EPOLL_CTL_ADD, m_InotifyHandle, &ev) < 0 )
        {
            m_InotifyHandle = -1;
            m_EpollHandle = -1;
        }
        else
        {
            LOG_INFO("New File Watcher created");
        }
    }
}

asge::filesystem::_linux::FileWatcher::~FileWatcher()
{
    Cancel();
    
    if ( m_EpollValid )
    {
        if ( ::close( m_EpollHandle ) < 0 )
        {
            LOG_ERROR( "Unable to close epoll handle: {}", std::strerror(errno) );
            return;
        }
    }

    Join();

    if ( m_InotifyValid )
    {
        if ( ::close( m_InotifyHandle ) < 0 )
        {
            LOG_ERROR( "Unable to close inotify handle: {}", std::strerror(errno) );
        }
    }

    if ( !isValid() ) return;

    for ( auto const [path, fd] : m_WatchFds )
    {
        if ( close( fd ) < 0 )
        {
            str::String u8_path = str::ToUTF8( path.u8string() );
            LOG_ERROR( "Unable to close handle for path {}: {}", 
                u8_path, std::strerror(errno) );
        }
    }
}

asge::Result<WatcherHandler> asge::filesystem::_linux::FileWatcher::AddWatch( 
    path_type inPath, _internal::callback_type_cref inCallback, mask_type inMask
) {
    // The file watcher must be a valid instance, otherwise we cannot add anything
    if ( !isValid() ) return Result<WatcherHandler>::Err(
        make_error_code( errors::FileWatcherError::InvalidFwHandle )
    );

    // Check if the input path actually exists
    if ( !meta::Exists( inPath ) )
    {
        auto const ec = std::make_error_code( std::errc::no_such_file_or_directory );
        return asge::Result<WatcherHandler>::Err( ec, str::ToUTF8( inPath.u8string() ) );
    }

    if ( meta::IsDirectory( inPath ) )
    {
        if ( auto result = RegisterNewWatch( inPath ); !result ) {
            return Result<WatcherHandler>::Err( result.Error() );
        }

        auto connector = m_OnEvent.Connect( _internal::CreateCallback( inCallback, inMask ) );
        return Result<WatcherHandler>::Ok(connector);
    }
    
    // First normalize the path, then take the parent folder and the filename
    inPath = inPath.lexically_normal();
    path_type parentPath = inPath.parent_path();
    if ( auto result = RegisterNewWatch( parentPath ); !result ) {
        return Result<WatcherHandler>::Err( result.Error() );
    }

    // Creates a new callback that filters the event by the path name
    auto filtered = [ inCallback, inPath ]( FileEvent const& e ) {
        if ( !e.IsDir() && e.s_Path == inPath ) inCallback( e );
    };

    // Registers the callback into the signals
    LOG_INFO("Added new watch for ", inPath, " with mask ", inMask);
    auto connector = m_OnEvent.Connect( _internal::CreateCallback( filtered, inMask ) );
    return Result<WatcherHandler>::Ok(connector);
}

#endif
