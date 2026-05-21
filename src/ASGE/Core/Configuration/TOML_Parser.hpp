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

namespace asge::config
{

namespace _internal
{

/**
 * Key values can either be: strings, doubles, integer, boolean, array
 */
struct toml_array;

using TOML_ValueType = std::variant<
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

std::ostream& operator<<( std::ostream& oss, TOML_ValueType const& inValue ) noexcept;

struct toml_array 
{
    std::vector<TOML_ValueType> elements;
};

enum class TOML_TableType
{
    Root,       // It is a root table of an array of tables of the same type
    Standard,   // Meaning that there will be no more tables of the same type
    Array       // It is a grouping of tables
};

enum class TOML_ElementType { Null, Int, Double, Bool, String, Array };

class TOML_Table : public std::enable_shared_from_this<TOML_Table>
{
private:
    std::string m_Name; // The name of the TOML table
    std::unordered_map<std::string, TOML_ValueType> m_kvPairs; // Key-Value pairs
    std::vector<std::shared_ptr<TOML_Table>> m_SubTables; // A vector of possible subtables
    std::weak_ptr<TOML_Table> m_ParentTable; // The parent table
    TOML_TableType m_Type; // Table type wrt arrays of tables

    std::string GetAbsoluteName() const noexcept;
    void PrintTable(std::ostream& oss) const noexcept;
public:
    TOML_Table(std::string const& inName, TOML_TableType inType)
    : m_Name( inName ), m_Type( inType )
    {}

    TOML_Table(TOML_TableType inType) : m_Name( "" ), m_Type( inType ) {}

    /* Delete all copy and move constructors and ass. operators */
    TOML_Table(TOML_Table const&)            = delete;
    TOML_Table& operator=(TOML_Table const&) = delete;
    TOML_Table(TOML_Table&&)                 = default;
    TOML_Table& operator=(TOML_Table&&)      = default;

    ~TOML_Table() = default;

    void AddKvPair( std::string const& inKey, TOML_ValueType const& inValue ) noexcept;
    void AddSubTable( std::shared_ptr<TOML_Table> inChild ) noexcept;
    void SetParent( std::weak_ptr<TOML_Table> inParent ) noexcept;

    friend std::ostream& operator<<( std::ostream& oss, TOML_Table const& inTable ) noexcept;
};

std::vector<std::string_view> TOML_SplitArrayElements( std::string_view inStr );
TOML_ValueType TOML_ParseArray( std::string_view inLine );
std::string TOML_ParseString( std::istringstream& inStream, std::string_view inLine );
TOML_ValueType TOML_ValueParse(std::istringstream& inStream, std::string_view inLine);
TOML_Table TOML_Parse(std::string const& inRaw) noexcept;
TOML_ElementType TOML_DetectType( std::string_view inLine ) noexcept;

}
}