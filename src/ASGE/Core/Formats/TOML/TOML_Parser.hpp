#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <variant>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <tuple>
#include <optional>
#include <type_traits>
#include <cstdint>

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Traits.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/FileData.hpp>

namespace asge::formats
{

namespace toml
{

namespace _internal
{

/**
 * @brief A TOML value: forward declaration of the wrapper around ValueType.
 *
 * Key values can either be: strings, doubles, integer, boolean, array.
 */
struct TOMLValue;

using ValueType = std::variant<
    std::string,
    double,
    int,
    std::int64_t,
    bool,
    std::vector<TOMLValue>
>;

// A ValueType that may recursively hold an array of itself (nested arrays).
struct TOMLValue : public ValueType
{
    using ValueType::ValueType;

    /**
     * @brief Wraps a ValueType by move.
     */
    TOMLValue(ValueType&& inValue) : ValueType(std::move(inValue)) {}

    /**
     * @brief Wraps a copy of a ValueType.
     */
    TOMLValue(ValueType const& inValue) : ValueType(inValue) {}
};

template<typename T> using VectorResult = Result<std::vector<T>>;

/**
 * @brief Converts a TOML array into a std::vector<T>, checking every element's type.
 *
 * Fails with ConfError::TomlTypeMismatch (or TomlExpectedNestedArray when T is
 * itself a vector and an element is not an array), naming the offending index.
 * Nested vectors are converted recursively.
 */
template<typename T>
VectorResult<T> ToTypedVector(std::vector<TOMLValue> const& inVec)
{
    std::vector<T> outResult;
    outResult.reserve( inVec.size() );

    for ( auto const& inElement : inVec )
    {
        if constexpr ( asge::_internal::traits::is_vector_v<T> )
        {
            // nested — recurse into inner vector<TOMLValue>
            auto* row = std::get_if<std::vector<TOMLValue>>(&inElement);
            if (!row) {
                auto ec = make_error_code( errors::ConfError::TomlExpectedNestedArray );
                std::string detail = "at index" + std::to_string(&inElement - &inVec[0]);
                return VectorResult<T>::Err( ec, detail );
            }

            // Recurse and check error for recursive call
            auto result = ToTypedVector<typename T::value_type>(*row);
            if (!result) return VectorResult<T>::Err( result.Error() );
            outResult.push_back(std::move(result.Value()));
        }
        else
        {
            // scalar or string
            auto* val = std::get_if<T>(&inElement);
            if (!val) {
                auto ec = make_error_code( errors::ConfError::TomlTypeMismatch );
                std::string detail = "at index" + std::to_string(&inElement - &inVec[0]);
                return VectorResult<T>::Err( ec, detail );
            }

            outResult.push_back(*val);
        }
    }

    return VectorResult<T>::Ok( std::move(outResult) );
}

enum class TableType
{
    Root,       // It is a root table of an array of tables of the same type
    Standard,   // Meaning that there will be no more tables of the same type
    Array       // It is a grouping of tables
};

enum class ElementType { Null, Int, Int64, Double, Bool, String, Array };

enum class StringType {
    Basic,          // "..."
    Literal,        // '...'
    MultiBasic,     // """..."""
    MultiLiteral,   // '''...'''
    Invalid
};

/**
 * Carries the original TOML formatting of a value (string quoting style,
 * numeric kind, per-element info for arrays) so that Save() can reproduce
 * it, since a bare ValueType cannot distinguish e.g. a literal string from
 * a basic one, or a whole-number double from an int.
 */
struct TOMLTypeInfo
{
    ElementType s_Type = ElementType::Null;                // The value's TOML kind
    StringType  s_StringType = StringType::Invalid; // meaningful only if s_Type == String
    std::vector<TOMLTypeInfo> s_ElementInfo{}; // meaningful only if s_Type == Array
};

// One key's value together with the formatting info needed to write it back.
struct TOMLEntry
{
    ValueType    s_Value; // The parsed value
    TOMLTypeInfo s_Info;  // How the value was written in the source
};

/**
 * @brief The TOMLTypeInfo to use for a newly created value of type T.
 *
 * Strings default to the basic ("...") style, vectors recurse into their
 * element type, and an unsupported T fails to compile.
 */
template<typename T>
TOMLTypeInfo DefaultTypeInfoFor() noexcept
{
    if constexpr ( std::is_same_v<T, int> ) return TOMLTypeInfo{ ElementType::Int };
    else if constexpr ( std::is_same_v<T, std::int64_t> ) return TOMLTypeInfo{ ElementType::Int64 };
    else if constexpr ( std::is_same_v<T, double> ) return TOMLTypeInfo{ ElementType::Double };
    else if constexpr ( std::is_same_v<T, bool> ) return TOMLTypeInfo{ ElementType::Bool };
    else if constexpr ( std::is_same_v<T, std::string> ) return TOMLTypeInfo{ ElementType::String, StringType::Basic };
    else if constexpr ( asge::_internal::traits::is_vector_v<T> )
        return TOMLTypeInfo{ ElementType::Array, StringType::Invalid, { DefaultTypeInfoFor<typename T::value_type>() } };
    else
        static_assert( asge::_internal::traits::always_false_v<T>, "unsupported TOML element type" );
}

class Table; // Forward declare table class

}

/** @brief Shared handle to a parsed/built TOML table node. */
using table_pointer = std::shared_ptr<_internal::Table>;

namespace _internal
{

/**
 * @brief Finds the direct subtable called inName (the last one for an array of tables).
 *
 * Returns TomlNoSubtable if there is none.
 */
Result<table_pointer> FindSubTable( table_pointer inParent, std::string const& inName ) noexcept;

/**
 * @brief Walks a dotted path from inRoot, creating every missing table along the way.
 *
 * inType is the type given to a table that has to be created at the end of the path.
 */
table_pointer FindOrCreateTable( table_pointer inRoot, std::string_view inTableName, TableType inType );

class Table : public std::enable_shared_from_this<Table>
{
public:
    using pointer = typename std::shared_ptr<Table>;

private:
    std::string m_Name; // The name of the TOML table
    std::unordered_map<std::string, TOMLEntry> m_kvPairs; // Key-Value pairs
    std::vector<pointer> m_SubTables; // A vector of possible subtables
    std::weak_ptr<Table> m_ParentTable; // The parent table
    TableType m_Type; // Table type wrt arrays of tables

    /**
     * @brief The dotted path from the root to this table, used for headers when printing.
     */
    std::string GetAbsoluteName() const noexcept;

    /**
     * @brief Writes this table and its subtables as TOML text.
     *
     * Key/value pairs come first, then each subtable under its own header.
     */
    void PrintTable(std::ostream& oss) const noexcept;

    /**
     * @brief Resolves a dotted path to the TOMLEntry of an already-existing key.
     *
     * Recurses into subtables exactly like Get<T>/GetTable do. Shared by
     * Set/SetTypedArray so the path-walking logic (and its error codes) lives
     * in exactly one place.
     */
    Result<TOMLEntry*> FindEntry( std::string const& inPath ) noexcept;
public:
    /**
     * @brief Creates an empty table called inName with the given array-of-tables kind.
     */
    Table(std::string const& inName, TableType inType)
    : m_Name( inName ), m_Type( inType )
    {}

    /**
     * @brief Creates an empty unnamed table (typically the document root).
     */
    Table(TableType inType) : m_Name( "" ), m_Type( inType ) {}

    /* Delete all copy and move constructors and ass. operators */
    Table(Table const&)            = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    Table& operator=(Table const&) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    Table(Table&&)                 = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    Table& operator=(Table&&)      = default;

    /**
     * @brief Destructor.
     *
     * Defaulted; members clean up after themselves.
     */
    ~Table() = default;

    /**
     * @brief Adds a new key with its entry.
     *
     * Fails with TomlAlreadyExistingKey if the key is already there; use Set
     * to change an existing value.
     */
    BoolResult AddKvPair( std::string const& inKey, TOMLEntry inEntry ) noexcept;

    /**
     * @brief Appends inChild to this table's subtables.
     */
    void AddSubTable( pointer inChild ) noexcept;

    /**
     * @brief Records the parent table (held weakly, so no ownership cycle).
     */
    void SetParent( std::weak_ptr<Table> inParent ) noexcept;

    /**
     * @brief Prints the table as TOML text; the output parses back to the same data.
     */
    inline friend std::ostream& operator<<( std::ostream& oss, Table const& inTable ) noexcept
    {
        inTable.PrintTable( oss );
        return oss;
    }

    /**
     * @brief The direct subtables, in document order.
     */
    std::vector<pointer> const& GetSubTables() const noexcept;

    /**
     * @brief The table's own name (empty for the root).
     */
    std::string const& GetName() const noexcept;

    /**
     * @brief Finds a subtable by dotted path, honoring `name[index]` segments.
     *
     * Read-only: never creates anything. Returns TomlNoSubtable if a segment
     * does not exist.
     */
    Result<pointer> GetTable( std::string const& inPath ) const noexcept;

    /**
     * @brief Reads the value at a dotted key path as type RetType.
     *
     * Returns a pointer into the table, valid until the table is modified.
     * Fails with TomlNoAttribute for a missing key or TomlTypeMismatch when
     * the stored value is of another type. RetType must be one of ValueType's
     * alternatives.
     */
    template<typename RetType>
    requires asge::_internal::traits::variant_contains_v<RetType, ValueType>
    Result<RetType const*> Get( std::string const& inPath ) const
    {
        std::string_view svPath = str::Trim(inPath);

        auto sepPos = svPath.find('.');
        if ( sepPos != std::string_view::npos )
        {
            auto child = GetTable(std::string(svPath.substr(0, sepPos)));
            if ( !child )
            {
                return Result<RetType const*>::Err( child.Error() );
            }

            return child.Value()->Get<RetType>(std::string(svPath.substr(sepPos + 1)));
        }

        auto kvIt = m_kvPairs.find(std::string(svPath));
        if ( kvIt == m_kvPairs.end() ) 
        {
            return Result<RetType const*>::Err( 
                make_error_code( errors::ConfError::TomlNoAttribute ), 
                std::string(svPath)
            );
        }

        if ( !std::holds_alternative<RetType>(kvIt->second.s_Value) )
        {
            return Result<RetType const*>::Err(
                make_error_code( errors::ConfError::TomlTypeMismatch )
            );
        }

        return Result<RetType const*>::Ok(std::get_if<RetType>(&kvIt->second.s_Value));
    }

    /**
     * @brief Reads an array key as a std::vector<T>.
     *
     * A missing key (or one that is not an array) yields an empty vector
     * rather than an error; an element of the wrong type is an error.
     */
    template<typename T>
    VectorResult<T> GetTypedArray( std::string const& inPath ) const
    {
        auto raw = Get<std::vector<TOMLValue>>( inPath );
        if (!raw) return VectorResult<T>::Ok(std::vector<T>{});
        return ToTypedVector<T>(*raw.Value());
    }

    /**
     * @brief Overwrites the value of an already-existing key.
     *
     * Preserves its original TOMLTypeInfo (string subtype, numeric kind, ...).
     * Does not create missing keys or tables, and fails with TomlTypeMismatch
     * if T differs from the stored type.
     */
    template<typename T>
    requires asge::_internal::traits::variant_contains_v<T, ValueType>
    BoolResult Set( std::string const& inPath, T inValue ) noexcept
    {
        auto entryResult = FindEntry( inPath );
        if ( !entryResult ) return BoolResult::Err( entryResult.Error() );

        auto* entry = entryResult.Value();
        if ( !std::holds_alternative<T>(entry->s_Value) )
        {
            return BoolResult::Err( make_error_code( errors::ConfError::TomlTypeMismatch ) );
        }

        entry->s_Value = std::move(inValue);
        return BoolResult::Ok();
    }

    /**
     * @brief Overwrites an already-existing array-valued key with new elements.
     *
     * Reuses the array's existing per-element TOMLTypeInfo (or a sensible
     * default if the existing array was empty).
     */
    template<typename T>
    BoolResult SetTypedArray( std::string const& inPath, std::vector<T> const& inValues ) noexcept
    {
        auto entryResult = FindEntry( inPath );
        if ( !entryResult ) return BoolResult::Err( entryResult.Error() );

        auto* entry = entryResult.Value();
        if ( !std::holds_alternative<std::vector<TOMLValue>>(entry->s_Value) )
        {
            return BoolResult::Err( make_error_code( errors::ConfError::TomlTypeMismatch ) );
        }

        TOMLTypeInfo elemInfo = !entry->s_Info.s_ElementInfo.empty()
            ? entry->s_Info.s_ElementInfo.front()
            : DefaultTypeInfoFor<T>();

        std::vector<TOMLValue> newValues;
        newValues.reserve( inValues.size() );
        for ( auto const& element : inValues )
        {
            if constexpr ( asge::_internal::traits::is_vector_v<T> )
            {
                std::vector<TOMLValue> nested;
                nested.reserve( element.size() );
                for ( auto const& innerElement : element ) nested.emplace_back( innerElement );
                newValues.emplace_back( std::move(nested) );
            }
            else
            {
                newValues.emplace_back( element );
            }
        }

        entry->s_Value = std::move(newValues);
        entry->s_Info.s_ElementInfo.assign( inValues.size(), elemInfo );
        return BoolResult::Ok();
    }

    /**
     * @brief Like Set(), but creates the key if it doesn't exist yet.
     *
     * The new key gets a default TOMLTypeInfo. Only vivifies the leaf key, not
     * missing intermediate tables (a dotted inKey into a missing subtable still
     * errors) — pair with FindOrCreateTable for that, as TOMLBuilder does.
     */
    template<typename T>
    requires asge::_internal::traits::variant_contains_v<T, ValueType>
    BoolResult SetOrCreate( std::string const& inKey, T inValue ) noexcept
    {
        auto result = Set<T>( inKey, inValue );
        if ( result ) return result;
        if ( result.Error().s_Code != make_error_code( errors::ConfError::TomlNoAttribute ) )
        {
            return result;
        }

        return AddKvPair( inKey, TOMLEntry{ std::move(inValue), DefaultTypeInfoFor<T>() } );
    }

    /**
     * @brief Like SetTypedArray(), but creates the key if it doesn't exist yet.
     *
     * Seeds an empty array entry with the right per-element shape and
     * delegates the actual fill (nested-array recursion included) to
     * SetTypedArray, so that logic lives in one place.
     */
    template<typename T>
    BoolResult SetOrCreateTypedArray( std::string const& inKey, std::vector<T> const& inValues ) noexcept
    {
        if ( auto result = SetTypedArray<T>( inKey, inValues ); result ) return result;

        auto seedResult = AddKvPair(
            inKey, TOMLEntry{ std::vector<TOMLValue>{}, DefaultTypeInfoFor<std::vector<T>>() }
        );
        if ( !seedResult ) return seedResult;
        return SetTypedArray<T>( inKey, inValues );
    }
};

}

/**
 * @brief Parses a raw TOML document already in memory.
 *
 * Returns the root table, or the first syntax error found.
 */
Result<table_pointer> Parse(std::string const& inRaw) noexcept;

/**
 * @brief Reads inPath's contents and parses them as TOML.
 *
 * A read failure is returned as ReadText's own error, unchanged.
 */
Result<table_pointer> Parse(filesystem::Path const& inPath) noexcept;

}

}