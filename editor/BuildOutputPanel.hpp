#pragma once

/**
 * @brief Floating "Build Output" window showing the C++ project's compile and run output,
 *        colored by level. ioOpen is its close button's state: it is set false when closed.
 */
void DrawBuildOutputPanel( bool& ioOpen ) noexcept;
