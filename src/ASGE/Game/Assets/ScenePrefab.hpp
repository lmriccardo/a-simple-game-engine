#pragma once

#include <ASGE/Core/Formats/TOML/TOML_TableView.hpp>
#include <ASGE/Core/Filesystem/FileData.hpp>

namespace asge::game::asset
{

/**
 * @brief A parsed prefab file: one root entity plus its descendants.
 *
 * Uses the scene file format (`[[entity]]` tables). Holds the parsed TOML so
 * it can be instantiated many times without re-reading the file.
 */
struct ScenePrefab
{
    formats::toml::TOMLTableView m_Table; // Root table of the parsed file

    /**
     * @brief Wraps an already parsed and validated prefab table.
     *
     * No validation happens here; use Load() to get a checked prefab.
     */
    explicit ScenePrefab( formats::toml::TOMLTableView inTableView );

    /**
     * @brief Parses and validates the prefab file at a real filesystem path.
     *
     * Needs at least one `[[entity]]` and exactly one root, i.e. a single
     * entity with no Hierarchy or with a null parent. Fails with an AssetError
     * otherwise, or with the parse error if the TOML is malformed.
     */
    static Result<ScenePrefab> Load( filesystem::Path const& inPath );
};

}