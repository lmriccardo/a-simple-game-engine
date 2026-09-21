#pragma once

struct SDL_Window;

/**
 * @brief Subscribes to Logger's OnLog signal so DrawConsolePanel has a
 *        running history to show -- call once at startup, before the first
 *        DrawConsolePanel.
 */
void InitConsolePanel() noexcept;

/**
 * @brief Floating panel listing recent Logger entries, color-coded by level.
 * @param inWindow Passed through to the "Save Log" button's native save
 *                 dialog, so it can be shown modal to the editor window.
 */
void DrawConsolePanel( SDL_Window* inWindow ) noexcept;
