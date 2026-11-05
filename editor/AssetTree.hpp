#pragma once

#include <set>
#include <string>
#include <vector>

/** @brief The immediate contents of one folder of a set of virtual paths. */
struct FolderListing
{
    std::vector<std::string> m_Folders; // names of the subfolders, alphabetical
    std::vector<std::string> m_Files;   // names of the files, alphabetical
};

/**
 * @brief Lists what sits directly inside inFolder in inPaths (virtual paths such as
 *        "assets/textures/player.png"). inFolder is a folder path with no leading or trailing
 *        slash, or empty for the top level. Subfolders are reported by name only and are not
 *        merged into chains, so the result is one level of an ordinary file explorer.
 */
FolderListing ListFolder( std::set<std::string> const& inPaths, std::string const& inFolder );
