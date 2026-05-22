#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <variant>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iostream>

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Logger/Logger.hpp>

namespace asge::config::toml
{

namespace _internal
{

/**
 * Key values can either be: strings, doubles, integer, boolean, array
 */
struct toml_array;

using ValueType = std::variant<
    std::string,
    double,
    int,
    bool,
    std::vector<std::string>,
    std::vector<double>,
    std::vector<int>,
    std::vector<bool>,
    std::shared_ptr<toml_array>
>;

std::ostream& operator<<( std::ostream& oss, ValueType const& inValue ) noexcept;

struct toml_array 
{
    std::vector<ValueType> elements;
};

enum class TableType
{
    Root,       // It is a root table of an array of tables of the same type
    Standard,   // Meaning that there will be no more tables of the same type
    Array       // It is a grouping of tables
};

enum class ElementType { Null, Int, Double, Bool, String, Array };

class Table : public std::enable_shared_from_this<Table>
{
private:
    std::string m_Name; // The name of the TOML table
    std::unordered_map<std::string, ValueType> m_kvPairs; // Key-Value pairs
    std::vector<std::shared_ptr<Table>> m_SubTables; // A vector of possible subtables
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
    void AddSubTable( std::shared_ptr<Table> inChild ) noexcept;
    void SetParent( std::weak_ptr<Table> inParent ) noexcept;

    friend std::ostream& operator<<( std::ostream& oss, Table const& inTable ) noexcept;
};

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

std::vector<std::string_view> SplitArrayElements( std::string_view inStr );
ValueType ParseArray( std::string_view inLine );
std::string ParseString( std::istringstream* inStream, std::string_view inLine );
ValueType ParseValue(std::istringstream* inStream, std::string_view inLine);
Table Parse(std::string const& inRaw) noexcept;
ElementType DetectType( std::string_view inLine ) noexcept;

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

}
}