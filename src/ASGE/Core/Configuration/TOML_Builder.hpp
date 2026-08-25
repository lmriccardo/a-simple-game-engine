#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/Filesystem.hpp>

#include "TOML_Parser.hpp"

namespace asge::config
{

/**
 * @brief Fluent handle to one table node while building a TOML document.
 *        Set()/SetArray() auto-vivify the key on this table; Table()
 *        descends into (creating if needed) a subtable, scoping further
 *        calls to it. Returned by TOMLBuilder::Table() — not constructed
 *        directly by callers.
 */
class TOMLTableView
{
protected:
    _internal::toml::table_pointer m_Table;

public:
    explicit TOMLTableView( _internal::toml::table_pointer inTable ) noexcept
        : m_Table( std::move(inTable) )
    {}

    /**
     * @brief Sets a scalar key on this table, creating it if it doesn't
     *        exist yet. A failure (e.g. re-setting an existing key with a
     *        different type) is logged rather than breaking the chain.
     */
    template<typename T>
    requires asge::_internal::traits::variant_contains_v<T, _internal::toml::ValueType>
    TOMLTableView& Set( std::string const& inKey, T inValue )
    {
        m_Table->template SetOrCreate<T>( inKey, std::move(inValue) ).LogError();
        return *this;
    }

    /**
     * @brief Sets a float-valued key. TOML has no separate 32-bit float
     *        type — it's stored as the same `double` a TOML "float" always
     *        is — this overload just spares callers the cast at every call
     *        site (e.g. every component field that happens to be `float`).
     */
    TOMLTableView& Set( std::string const& inKey, float inValue )
    {
        return Set<double>( inKey, static_cast<double>(inValue) );
    }

    /**
     * @brief Sets an array-valued key on this table, creating it if it
     *        doesn't exist yet. Elements may themselves be std::vector<U>
     *        for a nested TOML array.
     */
    template<typename T>
    TOMLTableView& SetArray( std::string const& inKey, std::vector<T> const& inValues )
    {
        m_Table->template SetOrCreateTypedArray<T>( inKey, inValues ).LogError();
        return *this;
    }

    /**
     * @brief Descends into a dotted subtable path relative to this table,
     *        creating any missing tables along the way, and returns a view
     *        scoped to it.
     */
    TOMLTableView Table( std::string const& inPath )
    {
        return TOMLTableView(
            _internal::toml::FindOrCreateTable( m_Table, inPath, _internal::toml::TableType::Standard )
        );
    }

    /**
     * @brief Appends a new element to an array-of-tables (`[[inKey]]`) under
     *        this table and returns a view scoped to it. Unlike Table(), each
     *        call creates a fresh sibling rather than reusing one of the same
     *        name — the first call tags it Root, later calls Array, mirroring
     *        how the parser distinguishes successive `[[inKey]]` blocks.
     */
    TOMLTableView ArrayTable( std::string const& inKey )
    {
        auto existing = _internal::toml::FindSubTable( m_Table, inKey );
        auto newTable = std::make_shared<_internal::toml::Table>(
            inKey, existing ? _internal::toml::TableType::Array : _internal::toml::TableType::Root
        );

        newTable->SetParent( m_Table );
        m_Table->AddSubTable( newTable );
        return TOMLTableView( newTable );
    }

    /**
     * @brief Reads a scalar value at inKey on this table, returning
     *        inDefault if the key is missing or holds a different type —
     *        the read-side counterpart to Set().
     */
    template<typename T>
    requires asge::_internal::traits::variant_contains_v<T, _internal::toml::ValueType>
    T Get( std::string const& inKey, T inDefault = T{} ) const
    {
        auto result = m_Table->template Get<T>( inKey );
        return result ? *result.Value() : std::move(inDefault);
    }

    /**
     * @brief Reads a float-valued key — the read-side counterpart to the
     *        float overload of Set(). Stored/looked up as `double`
     *        underneath; see that overload's doc comment for why.
     */
    float Get( std::string const& inKey, float inDefault = 0.0f ) const
    {
        return static_cast<float>( Get<double>( inKey, static_cast<double>(inDefault) ) );
    }

    /**
     * @brief True if a subtable exists at inPath relative to this table.
     *        Unlike Table(), never creates one — safe to probe for an
     *        optional section before descending into it.
     */
    bool HasTable( std::string const& inPath ) const
    {
        return static_cast<bool>( m_Table->GetTable( inPath ) );
    }
};

/**
 * @brief Fluent builder for a fresh TOML document, backed by the same
 *        Table tree the parser produces — it serializes through the same
 *        Table::operator<< that ConfigurationManager::Save() uses, so the
 *        output round-trips like any hand-written TOML file.
 */
class TOMLBuilder : public TOMLTableView
{
public:
    TOMLBuilder()
        : TOMLTableView( std::make_shared<_internal::toml::Table>( _internal::toml::TableType::Root ) )
    {}

    /** @brief Serializes the built document to a TOML-formatted string. */
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << *m_Table;
        return oss.str();
    }

    /** @brief Serializes the built document and writes it out to inPath. */
    BoolResult SaveToFile( filesystem::Path const& inPath ) const
    {
        return filesystem::WriteText( inPath, ToString() );
    }
};

}
