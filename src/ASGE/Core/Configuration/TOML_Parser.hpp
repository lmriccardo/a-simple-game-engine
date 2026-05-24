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
#include <ASGE/Core/Logger/Logger.hpp>

namespace asge::config
{

namespace _internal::toml
{

template<typename T, typename Variant>
struct variant_contains;

template<typename T, typename ...Ts>
struct variant_contains<T, std::variant<Ts...>>
    : std::disjunction<std::is_same<T, Ts>...> 
{};

template<typename T, typename Variant>
constexpr bool variant_contains_v = variant_contains<T, Variant>::value;

template<typename T>
struct is_vector : std::false_type {};

template<typename T>
struct is_vector<std::vector<T>> : std::true_type {};

template<typename T>
constexpr bool is_vector_v = is_vector<T>::value;

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

template<typename T>
auto ToTypedVector(std::vector<TOMLValue> const& inVec)
{
    std::vector<T> outResult;
    outResult.reserve( inVec.size() );

    for ( auto const& inElement : inVec )
    {
        if constexpr ( is_vector_v<T> )
        {
            // nested — recurse into inner vector<TOMLValue>
            auto* row = std::get_if<std::vector<TOMLValue>>(&inElement);
            if (!row) {
                LOG_ERROR("ToTypedVector: expected nested array at index ", &inElement - &inVec[0]);
                return std::vector<T>{};
            }
            outResult.push_back(ToTypedVector<typename T::value_type>(*row));
        }
        else
        {
            // scalar or string
            auto* val = std::get_if<T>(&inElement);
            if (!val) {
                LOG_ERROR("ToTypedVector: type mismatch at index ", &inElement - &inVec[0]);
                return std::vector<T>{};
            }
            outResult.push_back(*val);
        }
    }

    return outResult;
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
table_pointer Parse(std::string const& inRaw) noexcept;
ValueType ParseValue(std::istringstream* inStream, std::string_view inLine);

// ----------------------- PARSING ARRAYS -----------------------


std::vector<std::string_view> SplitArrayElements( std::string_view inStr );
ValueType ParseArray( std::string_view inLine );
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

std::string ParseString( std::istringstream* inStream, std::string_view inLine );
StringType DetectStringType( std::string_view inSv ) noexcept;
std::string ProcessEscape( std::string_view inSv, bool isMultiline=false ) noexcept;
std::string ParseBasicString( std::string_view inLine, bool isLiteral=false ) noexcept;
std::string ParseMultiLineString( std::istringstream& inStream, 
    std::string_view inSv, bool isLiteral=false
);

// ----------------------- PARSING TABLES UTILITIES -----------------------

struct PathSegment 
{
    std::string_view name;
    std::optional<std::size_t> index;
};

PathSegment ParseSegment(std::string_view inSegment);

std::pair<std::string_view, std::string_view> SplitTablePath( std::string_view inTableName ) noexcept;
table_pointer FindSubTable( table_pointer inParent, std::string const& inName ) noexcept;
table_pointer FindSubTable( table_const_pointer inParent, std::string const& inName ) noexcept;
table_pointer FindSubTableAt( table_const_pointer inParent, std::string const& inName, std::size_t inIndex ) noexcept;
table_pointer FindOrCreateTable( table_pointer inRoot, std::string_view inTableName, TableType inType);
table_pointer ParseArrayTable( table_pointer inRoot, std::string_view inLine );
table_pointer ParseTable( table_pointer inRoot, std::string_view inLine );

class Table : public std::enable_shared_from_this<Table>
{
public:
    using pointer = typename std::shared_ptr<Table>;

private:
    std::string m_Name; // The name of the TOML table
    std::unordered_map<std::string, ValueType> m_kvPairs; // Key-Value pairs
    std::vector<pointer> m_SubTables; // A vector of possible subtables
    std::weak_ptr<Table> m_ParentTable; // The parent table
    TableType m_Type; // Table type wrt arrays of tables

    std::string GetAbsoluteName() const noexcept;
    void PrintTable(std::ostream& oss) const noexcept;
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

    void AddKvPair( std::string const& inKey, ValueType const& inValue ) noexcept;
    void AddSubTable( pointer inChild ) noexcept;
    void SetParent( std::weak_ptr<Table> inParent ) noexcept;

    friend std::ostream& operator<<( std::ostream& oss, Table const& inTable ) noexcept;

    std::vector<pointer> const& GetSubTables() const noexcept;
    std::string const& GetName() const noexcept;

    pointer GetTable( std::string const& inPath ) const noexcept;

    template<typename RetType>
    requires variant_contains_v<RetType, ValueType>
    RetType const* Get( std::string const& inPath ) const
    {
        std::string_view svPath = str::Trim(inPath);

        auto sepPos = svPath.find('.');
        if ( sepPos != std::string_view::npos )
        {
            auto child = GetTable(std::string(svPath.substr(0, sepPos)));
            if ( !child ) return nullptr;
            return child->Get<RetType>(std::string(svPath.substr(sepPos + 1)));
        }

        auto kvIt = m_kvPairs.find(std::string(svPath));
        if ( kvIt == m_kvPairs.end() ) {
            LOG_ERROR("No key '", svPath, "' in table '", m_Name, "'");
            return nullptr;
        }

        if ( !std::holds_alternative<RetType>(kvIt->second) ) {
            LOG_ERROR("Type mismatch at key '", svPath, "'");
            return nullptr;
        }

        return std::get_if<RetType>(&kvIt->second);
    }

    template<typename T>
    auto GetTypedArray( std::string const& inPath ) const
    {
        auto* raw = Get<std::vector<TOMLValue>>( inPath );
        if (!raw) return decltype(ToTypedVector<T>(*raw)){};
        return ToTypedVector<T>(*raw);
    }
};

}
}