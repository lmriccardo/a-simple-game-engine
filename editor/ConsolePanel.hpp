#pragma once

struct SDL_Window;

/**
 * @brief Subscribes to Logger's OnLog signal so DrawConsoleContents has a
 *        running history to show -- call once at startup, before the first
 *        DrawConsoleContents.
 */
void InitConsolePanel() noexcept;

/**
 * @brief Draws the recent Logger entries, color-coded by level, into the
 *        current ImGui window or child (BottomPanel hosts it).
 * @param inWindow Passed through to the "Save Log" button's native save
 *                 dialog, so it can be shown modal to the editor window.
 */
void DrawConsoleContents( SDL_Window* inWindow ) noexcept;
