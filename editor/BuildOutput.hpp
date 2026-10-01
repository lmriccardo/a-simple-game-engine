#pragma once

#include <cstddef>
#include <string>
#include <vector>

/** @brief How a line of build/run output is shown. */
enum class BuildLevel { Info, Warning, Error };

/** @brief One line of the C++ project's compile or run output. */
struct BuildLine
{
    BuildLevel  m_Level;
    std::string m_Text;
};

/**
 * @brief Adds a line to the build output, which is kept apart from the editor's own log.
 *        Safe to call from any thread; the oldest lines are dropped past a fixed cap.
 */
void AppendBuildOutput( BuildLevel inLevel, std::string inText );

/** @brief A copy of every line currently held, oldest first. */
[[nodiscard]] std::vector<BuildLine> SnapshotBuildOutput();

/** @brief Removes every line. */
void ClearBuildOutput();
