#pragma once

namespace asge::filesystem
{

enum class FEventType
{
    Accessed,       // The file was accessed for example for reading
    AttrChanged,    // Meta-data attributes of the file are changed
    Closed,         // An opened file either for reading/writing was closed
    Created,        // File was created in watched directory
    Deleted,        // File was deleted from watched directory
    Modified,       // File was modified ( for example writing into it )
    Moved,          // File was moved from to a folder
    Opened          // File was opened
};

struct FileEvent
{
    
};

}