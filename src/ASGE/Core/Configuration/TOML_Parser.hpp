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

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Traits.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::config
{

namespace _internal::toml
{

/**
 * Key values can either be: strings, doubles, integer, boolean, array
 */
struct TOMLValue;

using ValueType = std::variant<
    std::string,
    double,
    int,
    bool,
    std::vector<TOMLValue>
>;

std::ostream& operator<<( std::ostream& oss, ValueType const& inValue ) noexcept;

struct TOMLValue : public ValueType
{
    using ValueType::ValueType;

    // explicit conversion from ValueType
    TOMLValue(ValueType&& inValue) : ValueType(std::move(inValue)) {}
    TOMLValue(ValueType const& inValue) : ValueType(inValue) {}
};

template<typename T> using VectorResult = Result<std::vector<T>>;

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

enum class ElementType { Null, Int, Double, Bool, String, Array };

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
    ElementType s_Type = ElementType::Null;
    StringType  s_StringType = StringType::Invalid; // meaningful only if s_Type == String
    std::vector<TOMLTypeInfo> s_ElementInfo{}; // meaningful only if s_Type == Array
};

struct TOMLEntry
{
    ValueType    s_Value;
    TOMLTypeInfo s_Info;
};

template<typename T>
TOMLTypeInfo DefaultTypeInfoFor() noexcept
{
    if constexpr ( std::is_same_v<T, int> ) return TOMLTypeInfo{ ElementType::Int };
    else if constexpr ( std::is_same_v<T, double> ) return TOMLTypeInfo{ ElementType::Double };
    else if constexpr ( std::is_same_v<T, bool> ) return TOMLTypeInfo{ ElementType::Bool };
    else if constexpr ( std::is_same_v<T, std::string> ) return TOMLTypeInfo{ ElementType::String, StringType::Basic };
    else if constexpr ( asge::_internal::traits::is_vector_v<T> )
        return TOMLTypeInfo{ ElementType::Array, StringType::Invalid, { DefaultTypeInfoFor<typename T::value_type>() } };
    else
        static_assert( asge::_internal::traits::always_false_v<T>, "unsupported TOML element type" );
}

class Table; // Forward declare table class
using table_pointer = std::shared_ptr<Table>;
using table_const_pointer = std::shared_ptr<const Table>;

template<ElementType EType = ElementType::Null>
struct to_native_type { using type = void; };

#define DEF_TO_NATIVE_TYPE( eType, Native ) \
    template<> struct to_native_type<eType> { \
        using type = Native; \
    };

DEF_TO_NATIVE_TYPE( ElementType::Bool, bool )
DEF_TO_NATIVE_TYPE( ElementType::String, std::string )
DEF_TO_NATIVE_TYPE( ElementType::Int, int )
DEF_TO_NATIVE_TYPE( ElementType::Double, double )

template<ElementType EType>
using to_native_type_t = typename to_native_type<EType>::type;

ElementType DetectType( std::string_view inLine ) noexcept;
std::string_view StripComment( std::string_view inLine ) noexcept;
Result<table_pointer> Parse(std::string const& inRaw) noexcept;
Result<TOMLEntry> ParseValue(std::istringstream* inStream, std::string_view inLine);

// ----------------------- PARSING ARRAYS -----------------------


std::vector<std::string_view> SplitArrayElements( std::string_view inStr );
Result<TOMLEntry> ParseArray( std::string_view inLine );
template<ElementType EType>
ValueType BuildArray( std::vector<std::string_view>& inElements )
{
    using value_type = to_native_type_t<EType>;
    std::vector<value_type> outResult;
    for ( auto& element : inElements ) {
        outResult.push_back( std::get<value_type>(ParseValue( nullptr, element )) );
    }
    return outResult;
}

// ----------------------- PARSING STRINGS -----------------------

Result<str::String> ParseString( std::istringstream* inStream, std::string_view inLine );
StringType DetectStringType( std::string_view inSv ) noexcept;
std::string ProcessEscape( std::string_view inSv, bool isMultiline=false ) noexcept;
std::string ParseBasicString( std::string_view inLine, bool isLiteral=false ) noexcept;
std::string ParseMultiLineString( std::istringstream& inStream,
    std::string_view inSv, bool isLiteral=false
);

// ----------------------- SERIALIZING VALUES -----------------------

std::string EscapeForOutput( std::string_view inRaw, bool isMultiline ) noexcept;
void WriteValue( std::ostream& oss, ValueType const& inValue, TOMLTypeInfo const& inInfo ) noexcept;

// ----------------------- PARSING TABLES UTILITIES -----------------------

struct PathSegment 
{
    std::string_view name;
    std::optional<std::size_t> index;
};

PathSegment ParseSegment(std::string_view inSegment);

std::pair<std::string_view, std::string_view> SplitTablePath( std::string_view inTableName ) noexcept;
Result<table_pointer> FindSubTable( table_pointer inParent, std::string const& inName ) noexcept;
Result<table_pointer> FindSubTable( table_const_pointer inParent, std::string const& inName ) noexcept;
Result<table_pointer> FindSubTableAt( table_const_pointer inParent, std::string const& inName, std::size_t inIndex ) noexcept;
table_pointer FindOrCreateTable( table_pointer inRoot, std::string_view inTableName, TableType inType);
table_pointer ParseArrayTable( table_pointer inRoot, std::string_view inLine );
table_pointer ParseTable( table_pointer inRoot, std::string_view inLine );

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

    std::string GetAbsoluteName() const noexcept;
    void PrintTable(std::ostream& oss) const noexcept;

    /**
     * @brief Resolves a dotted path to the TOMLEntry of an already-existing
     *        key, recursing into subtables exactly like Get<T>/GetTable do.
     *        Shared by Set/SetTypedArray so the path-walking logic (and its
     *        error codes) lives in exactly one place.
     */
    Result<TOMLEntry*> FindEntry( std::string const& inPath ) noexcept;
public:
    Table(std::string const& inName, TableType inType)
    : m_Name( inName ), m_Type( inType )
    {}

    Table(TableType inType) : m_Name( "" ), m_Type( inType ) {}

    /* Delete all copy and move constructors and ass. operators */
    Table(Table const&)            = delete;
    Table& operator=(Table const&) = delete;
    Table(Table&&)                 = default;
    Table& operator=(Table&&)      = default;

    ~Table() = default;

    BoolResult AddKvPair( std::string const& inKey, TOMLEntry inEntry ) noexcept;
    void AddSubTable( pointer inChild ) noexcept;
    void SetParent( std::weak_ptr<Table> inParent ) noexcept;

    inline friend std::ostream& operator<<( std::ostream& oss, Table const& inTable ) noexcept
    {
        inTable.PrintTable( oss );
        return oss;
    }

    std::vector<pointer> const& GetSubTables() const noexcept;
    std::string const& GetName() const noexcept;

    Result<pointer> GetTable( std::string const& inPath ) const noexcept;

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

    template<typename T>
    VectorResult<T> GetTypedArray( std::string const& inPath ) const
    {
        auto raw = Get<std::vector<TOMLValue>>( inPath );
        if (!raw) return VectorResult<T>::Ok(std::vector<T>{});
        return ToTypedVector<T>(*raw.Value());
    }

    /**
     * @brief Overwrites the value of an already-existing key, preserving its
     *        original TOMLTypeInfo (string subtype, numeric kind, ...). Does
     *        not create missing keys or tables.
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
     * @brief Overwrites an already-existing array-valued key with new
     *        elements, reusing the array's existing per-element TOMLTypeInfo
     *        (or a sensible default if the existing array was empty).
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
};

}
}