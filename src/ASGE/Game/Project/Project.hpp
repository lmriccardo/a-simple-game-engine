#pragma once

#include <vector>

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/FileData.hpp>
#include <ASGE/Core/Strings.hpp>

namespace asge::game::project
{

/** @brief One virtual-root-to-real-directory binding listed in a `.asgeproject`. */
struct Mount
{
    str::String      m_Name;          // virtual root, e.g. "assets"
    filesystem::Path m_RealDirectory; // absolute, or resolved against the project file's folder
};

/** @brief The parts of a `.asgeproject` a running game needs: its mounts, scene list and main scene. */
struct ProjectData
{
    filesystem::Path         m_FilePath;
    std::vector<Mount>       m_Mounts;
    std::vector<filesystem::Path> m_Scenes;    // in file order
    filesystem::Path         m_MainScene;      // the scene a game starts on; empty when the file names none
    int                      m_TargetWidth = 0;  // game window size from [View]; 0 when the file sets none
    int                      m_TargetHeight = 0;
    int                      m_TargetFps = 0;    // game frame-rate cap from [View]; 0 when the file sets none
};

/**
 * @brief Parses inPath as a `.asgeproject` written by the editor. Relative mount and
 *        scene paths are resolved against the file's own folder, so a project
 *        folder can be moved as a whole. Touches no engine state.
 */
Result<ProjectData> LoadProjectFile( filesystem::Path const& inPath ) noexcept;

}
