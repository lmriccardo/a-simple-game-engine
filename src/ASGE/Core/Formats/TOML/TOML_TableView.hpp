#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ASGE/Core/Errors.hpp>

#include "TOML_Parser.hpp"

namespace asge::formats::toml
{

/**
 * @brief Fluent handle to one table node while building or reading a TOML document.
 *
 * Set()/SetArray() auto-vivify the key on this table; Table() descends into
 * (creating if needed) a subtable, scoping further calls to it. Returned by
 * TOMLBuilder::Table() — not constructed directly by callers.
 */
class TOMLTableView
{
protected:
    table_pointer m_Table; // The table node this view is scoped to

public:
    /**
     * @brief Wraps inTable; the view shares ownership of the node.
     */
    explicit TOMLTableView( table_pointer inTable ) noexcept
        : m_Table( std::move(inTable) )
    {}

    /**
     * @brief Sets a scalar key on this table, creating it if it doesn't exist yet.
     *
     * A failure (e.g. re-setting an existing key with a different type) is
     * logged rather than breaking the chain.
     */
    template<typename T>
    requires asge::_internal::traits::variant_contains_v<T, _internal::ValueType>
    TOMLTableView& Set( std::string const& inKey, T inValue )
    {
        m_Table->template SetOrCreate<T>( inKey, std::move(inValue) ).LogError();
        return *this;
    }

    /**
     * @brief Sets a float-valued key.
     *
     * TOML has no separate 32-bit float type — it's stored as the same
     * `double` a TOML "float" always is — this overload just spares callers
     * the cast at every call site (e.g. every component field that happens to
     * be `float`).
     */
    TOMLTableView& Set( std::string const& inKey, float inValue )
    {
        return Set<double>( inKey, static_cast<double>(inValue) );
    }

    /**
     * @brief Sets an array-valued key on this table, creating it if it doesn't exist yet.
     *
     * Elements may themselves be `std::vector<U>` for a nested TOML array.
     */
    template<typename T>
    TOMLTableView& SetArray( std::string const& inKey, std::vector<T> const& inValues )
    {
        m_Table->template SetOrCreateTypedArray<T>( inKey, inValues ).LogError();
        return *this;
    }

    /**
     * @brief Sets a float-array-valued key.
     *
     * Stored as an array of TOML doubles — the array-valued counterpart to the
     * float overload of Set(); see its doc comment for why.
     */
    TOMLTableView& SetArray( std::string const& inKey, std::vector<float> const& inValues )
    {
        return SetArray<double>( inKey, std::vector<double>( inValues.begin(), inValues.end() ) );
    }

    /**
     * @brief Descends into a dotted subtable path, creating any missing tables.
     *
     * Returns a view scoped to inPath, relative to this table. Unlike GetTable(),
     * it does not understand `[index]` segments: "entity[0]" is one literal table
     * name, so it silently creates a new sibling. Use GetTable() to read one back.
     */
    TOMLTableView Table( std::string const& inPath )
    {
        return TOMLTableView(
            _internal::FindOrCreateTable( m_Table, inPath, _internal::TableType::Standard )
        );
    }

    /**
     * @brief Appends a new element to an array-of-tables (`[[inKey]]`) under this table.
     *
     * Returns a view scoped to the new element. Unlike Table(), each call
     * creates a fresh sibling rather than reusing one of the same name — the
     * first call tags it Root, later calls Array, mirroring how the parser
     * distinguishes successive `[[inKey]]` blocks.
     */
    TOMLTableView ArrayTable( std::string const& inKey )
    {
        auto existing = _internal::FindSubTable( m_Table, inKey );
        auto newTable = std::make_shared<_internal::Table>(
            inKey, existing ? _internal::TableType::Array : _internal::TableType::Root
        );

        newTable->SetParent( m_Table );
        m_Table->AddSubTable( newTable );
        return TOMLTableView( newTable );
    }

    /**
     * @brief Reads a scalar value at inKey on this table.
     *
     * Returns inDefault if the key is missing or holds a different type — the
     * read-side counterpart to Set().
     */
    template<typename T>
    requires asge::_internal::traits::variant_contains_v<T, _internal::ValueType>
    T Get( std::string const& inKey, T inDefault = T{} ) const
    {
        auto result = m_Table->template Get<T>( inKey );
        return result ? *result.Value() : std::move(inDefault);
    }

    /**
     * @brief Reads an int64-valued key, with an int fallback.
     *
     * Falls back to an int-typed value before giving up to inDefault: the
     * parser stores any integer that fits in 32 bits as `int` (e.g. a packed
     * color with alpha 0), so a strict int64 lookup would silently return
     * inDefault.
     */
    std::int64_t Get( std::string const& inKey, std::int64_t inDefault ) const
    {
        if ( auto asInt64 = m_Table->template Get<std::int64_t>( inKey ) )
            return *asInt64.Value();

        if ( auto asInt = m_Table->template Get<int>( inKey ) )
            return static_cast<std::int64_t>( *asInt.Value() );

        return inDefault;
    }

    /**
     * @brief Reads a float-valued key, with an int fallback.
     *
     * Read-side counterpart of the float overload of Set(), stored as `double`.
     * Falls back to an int-typed value before inDefault, so a TOML `y = 528` reads
     * the same as `y = 528.0` in hand-authored files.
     */
    float Get( std::string const& inKey, float inDefault = 0.0f ) const
    {
        if ( auto asDouble = m_Table->template Get<double>( inKey ) )
            return static_cast<float>( *asDouble.Value() );

        if ( auto asInt = m_Table->template Get<int>( inKey ) )
            return static_cast<float>( *asInt.Value() );

        return inDefault;
    }

    /**
     * @brief Reads an array-valued key at inKey on this table.
     *
     * Returns inDefault if the array's elements hold a different type — the
     * read-side counterpart to SetArray(). A missing key yields an empty
     * vector rather than inDefault, mirroring the underlying
     * Table::GetTypedArray().
     */
    template<typename T>
    std::vector<T> GetArray( std::string const& inKey, std::vector<T> inDefault = {} ) const
    {
        auto result = m_Table->template GetTypedArray<T>( inKey );
        return result ? std::move(result).Value() : std::move(inDefault);
    }

    /**
     * @brief Reads a float-array-valued key.
     *
     * The read-side counterpart to the float overload of SetArray(). Stored
     * and looked up as an array of doubles; see that overload's doc comment
     * for why.
     */
    std::vector<float> GetArray( std::string const& inKey, std::vector<float> inDefault = {} ) const
    {
        auto asDouble = GetArray<double>( inKey, std::vector<double>( inDefault.begin(), inDefault.end() ) );

        std::vector<float> result;
        result.reserve( asDouble.size() );
        for ( double value : asDouble ) result.push_back( static_cast<float>(value) );
        return result;
    }

    /**
     * @brief Read-only descent into an existing subtable at inPath.
     *
     * Honors `[index]` segments into an array-of-tables (e.g. `"entity[1]"`)
     * the same way Table::GetTable does. Unlike Table(), never creates
     * anything — this is what a loader walking `entity[0]`, `entity[1]`, ...
     * array elements back out of a parsed document needs; HasTable() is built
     * on top of it.
     */
    [[nodiscard]] Result<TOMLTableView> GetTable( std::string const& inPath ) const
    {
        auto table = m_Table->GetTable( inPath );
        if ( !table ) return Result<TOMLTableView>::Err( table.Error() );
        return Result<TOMLTableView>::Ok( TOMLTableView( table.Value() ) );
    }

    /**
     * @brief Read-only descent into element inIndex of an array-of-tables.
     *
     * Equivalent to GetTable(inPath + "[" + inIndex + "]"), for callers walking
     * a known count of elements (e.g. Serializer<T>::FromToml over
     * "Frame[0]", "Frame[1]", ...) without building that string themselves.
     */
    [[nodiscard]] Result<TOMLTableView> GetTable( std::string const& inPath, int inIndex ) const
    {
        return GetTable( inPath + "[" + std::to_string(inIndex) + "]" );
    }

    /**
     * @brief True if a subtable exists at inPath relative to this table.
     *
     * Unlike Table(), never creates one — safe to probe for an optional
     * section before descending into it.
     */
    bool HasTable( std::string const& inPath ) const
    {
        return static_cast<bool>( GetTable( inPath ) );
    }
};

}
