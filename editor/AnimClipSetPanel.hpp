#pragma once

#include <filesystem>

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>

struct SDL_Window;

/** @brief Loads a clip set file into the Animation Clip Set panel; the caller shows the window. */
void OpenAnimClipSet( std::filesystem::path const& inPath ) noexcept;

/**
 * @brief Draws the floating "Animation Clip Set" window.
 *
 * Binds each clip name a state machine declares to a frame table and a frame
 * duration. The machine is picked from the known state machines and read
 * through inVfs for its clip names. Saving goes through WriteClipSet, so a set
 * the engine would reject is never written.
 */
void DrawAnimClipSetPanel(
    bool& ioOpen, SDL_Window* inWindow, asge::filesystem::VirtualFileSystem const& inVfs,
    asge::ecs::Registry& inRegistry ) noexcept;
