#include "TOML_Parser.hpp"

using namespace asge::config::toml::_internal;

std::string asge::config::toml::_internal::Table::GetAbsoluteName() const noexcept
{
    if ( auto pTable = m_ParentTable.lock() )
    {
        auto pName = pTable->GetAbsoluteName();
        if ( pName.empty() ) return m_Name;
        return pName + "." + m_Name;
    }

    return m_Name;
}

void asge::config::toml::_internal::Table::PrintTable(std::ostream &oss) const noexcept
{
    // print header
    if (!m_Name.empty()) {
        switch (m_Type) {
        case TableType::Standard:
            oss << "["  << GetAbsoluteName() << "]\n";
            break;
        case TableType::Root:
        case TableType::Array:
            oss << "[[" << GetAbsoluteName() << "]]\n";
            break;
        }
    }

    // print key-value pairs
    for (auto const& [key, value] : m_kvPairs) 
    {
        oss << key << " = " << value << "\n";
    }

    // recurse into subtables
    for (auto const& sub : m_SubTables)
        sub->PrintTable(oss);
}

void asge::config::toml::_internal::Table::AddKvPair(
    std::string const &inKey, ValueType const &inValue) noexcept
{
    if ( m_kvPairs.find( inKey ) != m_kvPairs.end() )
    {
        LOG_ERROR("Key " + inKey + " already present in " + m_Name + " table");
        return;
    }

    m_kvPairs[inKey] = inValue;
}

void asge::config::toml::_internal::Table::AddSubTable(std::shared_ptr<Table> inChild) noexcept
{
    m_SubTables.push_back( inChild );
    inChild->SetParent( shared_from_this() );
}

void asge::config::toml::_internal::Table::SetParent(std::weak_ptr<Table> inParent) noexcept
{
    m_ParentTable.swap( inParent );
}

std::ostream &asge::config::toml::_internal::operator<<(std::ostream &oss, ValueType const &inValue) noexcept
{
    std::visit([&oss](auto&& v) {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, std::string>)
            oss << '"' << v << '"';

        else if constexpr (std::is_same_v<T, bool>)
            oss << (v ? "true" : "false");

        else if constexpr (std::is_same_v<T, std::shared_ptr<toml_array>>) {
            oss << "[";
            for (size_t i = 0; i < v->elements.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << v->elements[i];  // recurse
            }
            oss << "]";
        }

        else if constexpr (std::is_same_v<T, std::vector<std::string>> ||
                           std::is_same_v<T, std::vector<double>>       ||
                           std::is_same_v<T, std::vector<int>>          ||
                           std::is_same_v<T, std::vector<bool>>) {
            oss << "[";
            for (size_t i = 0; i < v.size(); ++i) {
                if (i > 0) oss << ", ";
                if constexpr (std::is_same_v<T, std::vector<std::string>>)
                    oss << '"' << v[i] << '"';
                else if constexpr (std::is_same_v<T, std::vector<bool>>)
                    oss << (v[i] ? "true" : "false");
                else
                    oss << v[i];
            }
            oss << "]";
        }

        else
            oss << v;  // int, double

    }, inValue);

    return oss;
}

std::ostream &asge::config::toml::_internal::operator<<(std::ostream &oss, Table const &inTable) noexcept
{
    inTable.PrintTable( oss );
    return oss;
}

std::vector<std::string_view> asge::config::toml::_internal::SplitArrayElements(std::string_view inStr)
{
    // This function already assumes that the input string already
    // have outer brackets stripped
    std::vector<std::string_view> outResult;
    
    int depth          = 0;      // Trackes nested arrays
    bool inString      = false;  // Detects if scanning is still inside a string
    char quote         = 0;      // The quote starting the string element
    std::size_t eStart = 0;      // Starts index of the current element

    for ( std::size_t ii = 0; ii < inStr.size(); ++ii )
    {
        char currChar = inStr[ ii ];

        if ( !inString && ( currChar == '"' || currChar == '\'' ) )
        {
            // If we are not in a string and a quote appears then this is
            // likely a start of a string element
            inString = true;
            quote = currChar;
        }
        else if ( inString && currChar == quote && ( ii == 0 || inStr[ii - 1] != '\\' ) )
        {
            // Matches ends of strings
            inString = false;
        }
        else if ( !inString && currChar == '[' )
        {
            // Matches possible starts of subarrays. Possible only if we are
            // not into a string currently.
            ++depth;
        }
        else if ( !inString && currChar == ']' )
        {
            // Matches possible end of subarrays ( not into a string )
            --depth;
        }
        else if ( !inString && depth == 0 && currChar == ',' )
        {
            // Delimiter reached on depth 0 and outside a possible string
            auto cElement = str::Trim( inStr.substr( eStart, ii - eStart ) );
            if ( !cElement.empty() ) outResult.push_back( cElement );
            eStart = ii + 1;
        }
    }

    // Do not forget to put the last element in the resulting vector
    auto lastElement = str::Trim( inStr.substr( eStart ) );
    if ( !lastElement.empty() ) outResult.push_back( lastElement );

    return outResult;
}

ValueType asge::config::toml::_internal::ParseArray(std::string_view inLine)
{
    if ( inLine.front() == '[' ) inLine.remove_prefix(1);
    if ( inLine.back()  == ']' ) inLine.remove_suffix(1);
    inLine = str::Trim( inLine );
    
    // Check that after stripping out brackets the resulting string
    // is not empty. If it is empty than we simply return an empty vector
    if ( inLine.empty() ) return std::vector<int>{};

    // Split the array string into multiple elements
    auto arrayElems = SplitArrayElements( inLine );
    if ( arrayElems.empty() ) return std::vector<int>{};

    // Detect the element type of the array by inspecting only the first element
    ElementType elemType = DetectType( arrayElems[0] );

    switch (elemType)
    {
    case ElementType::String: return BuildArray<ElementType::String>( arrayElems );
    case ElementType::Bool:   return BuildArray<ElementType::Bool>( arrayElems );
    case ElementType::Int:    return BuildArray<ElementType::Int>( arrayElems );
    case ElementType::Double: return BuildArray<ElementType::Double>( arrayElems );
    case ElementType::Array:
    {
        auto nested = std::make_shared<toml_array>();
        for ( auto const& element : arrayElems ) {
            nested->elements.push_back(ParseArray( element ));
        }
        return nested;
    }
    default:
        break;
    }
}

std::string asge::config::toml::_internal::ParseString(std::istringstream *inStream, std::string_view inLine)
{
    return std::string();
}

ValueType asge::config::toml::_internal::ParseValue(std::istringstream *inStream, std::string_view inLine)
{
    const auto tomlType = DetectType( inLine );

    switch (tomlType)
    {
    case ElementType::String: return ParseString(inStream, inLine);
    case ElementType::Array: return ParseArray(inLine);
    case ElementType::Bool: return inLine == "true";
    case ElementType::Double: return std::stod( std::string(inLine) );
    case ElementType::Int: return std::stoi( std::string(inLine) );
    default:
        throw std::runtime_error("bad TOML formatting!!");
    }
}

Table asge::config::toml::_internal::Parse(std::string const &inRaw) noexcept
{
    // Creates the root table
    auto rootTable = std::make_shared<Table>( TableType::Root );
    auto currTable = rootTable;

    std::istringstream stream( inRaw );
    std::string line;
    std::string_view svLine;

    while ( std::getline(stream, line) )
    {
        // Jump to the next line if the current one is empty
        if ( line.empty() ) continue;

        // Store the current line into a string_view to faster the parsing
        svLine = line;

        // Check if the line is a key_value pair, i.e., find the = symbol
        auto kvSep = svLine.find_first_of("=");
        if ( kvSep != std::string_view::npos )
        {
            auto kvKey    = str::Trim(svLine.substr(0, kvSep));
            auto kvValue  = str::Trim(svLine.substr(kvSep + 1, svLine.size()));
            auto kvParsed = ParseValue( &stream, kvValue );

            // Add the key-value pair into the current table
            currTable->AddKvPair( std::string(kvKey), kvParsed );
            continue;
        }

        // If the current line is not an assignment line, then we need to
        // remove the leading and ending whitespaces.
        svLine = str::Trim( svLine );
    }

    return std::move(*rootTable);
}

ElementType asge::config::toml::_internal::DetectType(std::string_view inLine) noexcept
{
    if (inLine.empty()) return ElementType::Null;

    if (inLine[0] == '[')                           return ElementType::Array;
    if (inLine[0] == '"' || inLine[0] == '\'')      return ElementType::String;
    if (inLine == "true")                           return ElementType::Bool;
    if (inLine == "false")                          return ElementType::Bool;
    if (inLine.find('.') != std::string_view::npos) return ElementType::Double;

    return ElementType::Int;
}
