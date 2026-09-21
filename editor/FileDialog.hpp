#pragma once

#include <SDL3/SDL_dialog.h>

#include <mutex>
#include <string>

/**
 * @brief Holds the async result of an SDL_Show{Open,Save}FileDialog call.
 *        The callback may run on a different thread than the main loop
 *        (SDL's own doc note), so the result is handed off through this
 *        mutex-guarded struct rather than acted on directly from the
 *        callback -- whatever the result drives (SceneManager, a file
 *        write, ...) happens back on the main thread, via DrainFileDialogResult.
 */
struct FileDialogResult
{
    std::mutex m_Mutex;
    bool m_Ready = false;
    bool m_Accepted = false; // false covers both "user canceled" and "error"
    std::string m_Path;
};

/** @brief SDL_DialogFileCallback for any FileDialogResult instance, passed in as userdata. */
void OnFileDialogResult( void* inUserdata, char const* const* inFileList, int inFilter ) noexcept;

/**
 * @brief Consumes inResult's queued result (if any) -- call once per frame,
 *        on the main thread, before acting on it.
 * @return True with outPath set if the user accepted a file this frame;
 *         false (outPath untouched) if nothing new arrived, or the dialog
 *         was canceled/errored.
 */
bool DrainFileDialogResult( FileDialogResult& inResult, std::string& outPath ) noexcept;
