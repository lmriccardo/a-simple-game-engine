#pragma once

#include <set>
#include <string>
#include <vector>

/** @brief One row of the Assets panel's folder tree: a folder (holding children) or an asset file. */
struct AssetTreeNode
{
    std::string                m_Name;                // what the row shows: a file's name, or a folder's path below its parent ("a/b" once chains collapse)
    std::string                m_Path;                // a file's full virtual path; a folder's full path from the root, unique per node
    bool                       m_IsFolder = false;
    std::vector<AssetTreeNode> m_Children;            // folders before files, each alphabetical
};

/**
 * @brief Builds the folder tree of inPaths (virtual paths such as "assets/textures/player.png").
 *        A folder holding exactly one folder and no files is merged with it, so a lone
 *        "assets/textures/characters" chain is one row rather than three nested ones.
 */
std::vector<AssetTreeNode> BuildAssetTree( std::set<std::string> const& inPaths );
