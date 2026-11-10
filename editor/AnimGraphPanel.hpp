#pragma once

#include <filesystem>

#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Components/Animator.hpp>

struct SDL_Window;

/** @brief Loads a state machine file into the Animation Graph panel; the caller shows the window. */
void OpenAnimGraph( std::filesystem::path const& inPath ) noexcept;

/**
 * @brief Draws the floating "Animation Graph" window.
 *
 * Opens a state machine file as a node graph: states are draggable boxes,
 * transitions are arrows, and the side column edits the selected state and
 * the flag, trigger and clip lists. Saving goes through WriteMachine, so a
 * machine the engine would reject is never written; box positions are saved
 * in the machine's layout sidecar. With ioPreview ticked, inAnimator (the
 * selected entity's, may be null) is shown live: its current state is
 * highlighted and its flags and triggers can be driven by hand.
 */
void DrawAnimGraphPanel(
    bool& ioOpen, SDL_Window* inWindow, bool& ioPreview, asge::game::components::Animator* inAnimator,
    asge::filesystem::VirtualFileSystem const& inVfs ) noexcept;
