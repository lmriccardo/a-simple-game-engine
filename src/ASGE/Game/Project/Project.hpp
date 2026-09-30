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

/** @brief The parts of a `.asgeproject` a running game needs: its mounts and its scene list. */
struct ProjectData
{
    filesystem::Path         m_FilePath;
    std::vector<Mount>       m_Mounts;
    std::vector<filesystem::Path> m_Scenes; // in file order; the first one is the start scene
};

/**
 * @brief Parses inPath as a `.asgeproject` written by the editor. Relative mount and
 *        scene paths are resolved against the file's own folder, so a project
 *        folder can be moved as a whole. Touches no engine state.
 */
Result<ProjectData> LoadProjectFile( filesystem::Path const& inPath ) noexcept;

}
