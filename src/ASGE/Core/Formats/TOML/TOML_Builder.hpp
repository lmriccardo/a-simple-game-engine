#pragma once

#include <sstream>
#include <string>

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/Filesystem.hpp>

#include "TOML_Parser.hpp"
#include "TOML_TableView.hpp"

namespace asge::formats::toml
{

/**
 * @brief Fluent builder for a fresh TOML document.
 *
 * Backed by the same Table tree the parser produces: it serializes through the
 * same Table::operator<< that ConfigurationManager::Save() uses, so the output
 * round-trips like any hand-written TOML file.
 */
class TOMLBuilder : public TOMLTableView
{
public:
    /**
     * @brief Starts an empty document (a root table with no keys).
     */
    TOMLBuilder()
        : TOMLTableView( std::make_shared<_internal::Table>( _internal::TableType::Root ) )
    {}

    /**
     * @brief Serializes the built document to a TOML-formatted string.
     */
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << *m_Table;
        return oss.str();
    }

    /**
     * @brief Serializes the built document and writes it out to inPath.
     *
     * Overwrites an existing file; returns WriteText's error on failure.
     */
    BoolResult SaveToFile( filesystem::Path const& inPath ) const
    {
        return filesystem::WriteText( inPath, ToString() );
    }
};

}
