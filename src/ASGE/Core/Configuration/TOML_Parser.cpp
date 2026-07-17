#include "TOML_Parser.hpp"

#include <cstdio>

using namespace asge::config::_internal::toml;

std::string asge::config::_internal::toml::Table::GetAbsoluteName() const noexcept
{
    if ( auto pTable = m_ParentTable.lock() )
    {
        auto pName = pTable->GetAbsoluteName();
        if ( pName.empty() ) return m_Name;
        return pName + "." + m_Name;
    }

    return m_Name;
}

void asge::config::_internal::toml::Table::PrintTable(std::ostream &oss) const noexcept
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
    for (auto const& [key, entry] : m_kvPairs)
    {
        oss << key << " = ";
        WriteValue( oss, entry.s_Value, entry.s_Info );
        oss << "\n";
    }

    // recurse into subtables
    for (auto const& sub : m_SubTables)
        sub->PrintTable(oss);
}

asge::BoolResult asge::config::_internal::toml::Table::AddKvPair(std::string const &inKey, TOMLEntry inEntry) noexcept
{
    if ( m_kvPairs.find( inKey ) != m_kvPairs.end() )
    {
        auto const ec = make_error_code( errors::ConfError::TomlAlreadyExistingKey );
        std::string detail = "key " + inKey + " table " + m_Name;
        return BoolResult::Err( ec, detail );
    }

    m_kvPairs[inKey] = std::move(inEntry);
    return BoolResult::Ok();
}

void asge::config::_internal::toml::Table::AddSubTable(Table::pointer inChild) noexcept
{
    m_SubTables.push_back( inChild );
    inChild->SetParent( shared_from_this() );
}

void asge::config::_internal::toml::Table::SetParent(std::weak_ptr<Table> inParent) noexcept
{
    m_ParentTable.swap( inParent );
}

std::vector<Table::pointer> const &asge::config::_internal::toml::Table::GetSubTables() const noexcept
{
    return m_SubTables;
}

std::string const &asge::config::_internal::toml::Table::GetName() const noexcept
{
    return m_Name;
}

asge::Result<Table::pointer> asge::config::_internal::toml::Table::GetTable(std::string const &inPath) const noexcept
{
    std::string_view svPath = str::Trim(inPath);

    auto sepPos  = svPath.find('.');
    auto segment = ParseSegment(
        sepPos == std::string_view::npos ? svPath : svPath.substr(0, sepPos));

    // resolve current segment — indexed or plain
    Result<table_pointer> child = segment.index.has_value()
        ? FindSubTableAt(shared_from_this(), std::string(segment.name), *segment.index)
        : FindSubTable(shared_from_this(), std::string(segment.name));

    if ( !child ) return Result<Table::pointer>::Err( child.Error() );

    // base case — no more segments
    if ( sepPos == std::string_view::npos ) return child;

    // recurse into remaining path
    return child.Value()->GetTable(std::string(svPath.substr(sepPos + 1)));
}

asge::Result<TOMLEntry*> asge::config::_internal::toml::Table::FindEntry(std::string const &inPath) noexcept
{
    std::string_view svPath = str::Trim(inPath);

    auto sepPos = svPath.find('.');
    if ( sepPos != std::string_view::npos )
    {
        auto child = GetTable(std::string(svPath.substr(0, sepPos)));
        if ( !child )
        {
            return Result<TOMLEntry*>::Err( child.Error() );
        }

        return child.Value()->FindEntry(std::string(svPath.substr(sepPos + 1)));
    }

    auto kvIt = m_kvPairs.find(std::string(svPath));
    if ( kvIt == m_kvPairs.end() )
    {
        return Result<TOMLEntry*>::Err(
            make_error_code( errors::ConfError::TomlNoAttribute ),
            std::string(svPath)
        );
    }

    return Result<TOMLEntry*>::Ok( &kvIt->second );
}

std::ostream &asge::config::_internal::toml::operator<<(std::ostream &oss, ValueType const &inValue) noexcept
{
    std::visit([&oss](auto&& v) {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, std::string>)
            oss << '"' << v << '"';

        else if constexpr (std::is_same_v<T, bool>)
            oss << (v ? "true" : "false");

        else if constexpr (std::is_same_v<T, std::vector<TOMLValue>>) {
            oss << "[";
            for (std::size_t i = 0; i < v.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << v[i];  // recurse — each element is a ValueType
            }
            oss << "]";
        }

        else
            oss << v;  // int, double

    }, inValue);

    return oss;
}

namespace
{

std::string FormatDouble(double inValue)
{
    std::ostringstream oss;
    oss << inValue;
    auto result = oss.str();
    if (result.find('.') == std::string::npos &&
        result.find('e') == std::string::npos &&
        result.find('E') == std::string::npos)
    {
        result += ".0";
    }
    return result;
}

// Best-effort fallback used only when a value's TOMLTypeInfo is missing/stale
// (e.g. array size mismatch) — infers a plain default straight from the
// runtime alternative actually held by the ValueType.
TOMLTypeInfo InferTypeInfo(ValueType const& inValue) noexcept
{
    return std::visit([](auto&& v) -> TOMLTypeInfo {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return TOMLTypeInfo{ ElementType::String, StringType::Basic };
        else if constexpr (std::is_same_v<T, bool>)    return TOMLTypeInfo{ ElementType::Bool };
        else if constexpr (std::is_same_v<T, int>)     return TOMLTypeInfo{ ElementType::Int };
        else if constexpr (std::is_same_v<T, double>)  return TOMLTypeInfo{ ElementType::Double };
        else return TOMLTypeInfo{ ElementType::Array };
    }, inValue);
}

}

std::string asge::config::_internal::toml::EscapeForOutput(std::string_view inRaw, bool isMultiline) noexcept
{
    std::string result;
    result.reserve( inRaw.size() );

    for ( unsigned char c : inRaw )
    {
        switch (c)
        {
        case '\\': result += "\\\\"; break;
        case '"':  result += "\\\""; break;
        case '\n': result += isMultiline ? "\n" : "\\n"; break;
        case '\r': result += isMultiline ? "\r" : "\\r"; break;
        case '\t': result += '\t'; break; // tab is allowed raw in both forms
        default:
            if ( c < 0x20 )
            {
                char buf[8];
                std::snprintf( buf, sizeof(buf), "\\u%04x", c );
                result += buf;
            }
            else
            {
                result += static_cast<char>(c);
            }
            break;
        }
    }

    return result;
}

void asge::config::_internal::toml::WriteValue(std::ostream &oss, ValueType const &inValue, TOMLTypeInfo const &inInfo) noexcept
{
    std::visit([&oss, &inInfo](auto&& v) {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, std::string>)
        {
            switch (inInfo.s_StringType)
            {
            case StringType::Literal:
                oss << '\'' << v << '\'';
                break;
            case StringType::MultiBasic:
                oss << "\"\"\"" << EscapeForOutput(v, true) << "\"\"\"";
                break;
            case StringType::MultiLiteral:
                oss << "'''" << v << "'''";
                break;
            case StringType::Basic:
            case StringType::Invalid:
            default:
                oss << '"' << EscapeForOutput(v, false) << '"';
                break;
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            oss << (v ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            oss << FormatDouble(v);
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            oss << v;
        }
        else if constexpr (std::is_same_v<T, std::vector<TOMLValue>>)
        {
            oss << "[";
            for ( std::size_t i = 0; i < v.size(); ++i )
            {
                if (i > 0) oss << ", ";
                TOMLTypeInfo const& childInfo = i < inInfo.s_ElementInfo.size()
                    ? inInfo.s_ElementInfo[i]
                    : InferTypeInfo(v[i]);
                WriteValue( oss, v[i], childInfo );
            }
            oss << "]";
        }

    }, inValue);
}

std::vector<std::string_view> asge::config::_internal::toml::SplitArrayElements(std::string_view inStr)
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

asge::Result<TOMLEntry> asge::config::_internal::toml::ParseArray(std::string_view inLine)
{
    if ( inLine.front() == '[' ) inLine.remove_prefix(1);
    if ( inLine.back()  == ']' ) inLine.remove_suffix(1);
    inLine = str::Trim( inLine );

    if ( inLine.empty() )
    {
        return Result<TOMLEntry>::Ok(TOMLEntry{
            std::vector<TOMLValue>{}, TOMLTypeInfo{ ElementType::Array }
        });
    }

    auto arrayElems = SplitArrayElements( inLine );
    if ( arrayElems.empty() )
    {
        return Result<TOMLEntry>::Ok(TOMLEntry{
            std::vector<TOMLValue>{}, TOMLTypeInfo{ ElementType::Array }
        });
    }

    std::vector<TOMLValue> result;
    std::vector<TOMLTypeInfo> elemInfos;
    result.reserve( arrayElems.size() );
    elemInfos.reserve( arrayElems.size() );
    for ( auto const& elem : arrayElems )
    {
        auto parse_result = ParseValue( nullptr, str::Trim(elem) );
        if ( !parse_result ) return Result<TOMLEntry>::Err( parse_result.Error() );
        result.push_back( std::move(parse_result.Value().s_Value) );
        elemInfos.push_back( std::move(parse_result.Value().s_Info) );
    }

    return Result<TOMLEntry>::Ok(TOMLEntry{
        std::move(result), TOMLTypeInfo{ ElementType::Array, StringType::Invalid, std::move(elemInfos) }
    });
}

asge::Result<asge::str::String> asge::config::_internal::toml::ParseString(
    std::istringstream *inStream, std::string_view inLine)
{
    StringType strType = DetectStringType( inLine );
    bool isLiteral = ( strType == StringType::Literal 
        || strType == StringType::MultiLiteral );

    str::String result;

    switch ( strType )
    {
    case StringType::Basic:
    case StringType::Literal:
    {
        result = ParseBasicString( inLine, isLiteral );
        break;
    }
    case StringType::MultiBasic:
    case StringType::MultiLiteral: 
    {
        result = ParseMultiLineString( *inStream, inLine, isLiteral );
        break;
    }
    default: break;
    }

    // If the resulting string is empty either it is an invalid
    // string format, or there is an empty string in the
    // configuration which is invalid by construction
    if ( result.empty() )
    {
        auto const ec = make_error_code( errors::ConfError::TomlInvalidString );
        return Result<str::String>::Err(ec);
    }

    return Result<str::String>::Ok( std::move(result) );
}

StringType asge::config::_internal::toml::DetectStringType(std::string_view inSv) noexcept
{
    if (inSv.starts_with(R"(""")")) return StringType::MultiBasic;
    if (inSv.starts_with("'''"))    return StringType::MultiLiteral;
    if (inSv.starts_with('"'))      return StringType::Basic;
    if (inSv.starts_with('\''))     return StringType::Literal;
    return StringType::Invalid;
}

std::string asge::config::_internal::toml::ProcessEscape(std::string_view inSv, bool isMultiline) noexcept
{
    std::string result;
    result.reserve( inSv.size() );

    auto mlFnCheck = []( char inChr ) -> bool
    {
        return inChr == '\n' || inChr == '\r' 
            || inChr == ' '  || inChr == '\t';
    };

    for ( std::size_t ii = 0; ii < inSv.size(); ++ii )
    {
        if ( inSv[ii] != '\\' )
        {
            result += inSv[ii];
            continue;
        }

        if ( ++ii >= inSv.size() ) return {}; // Dangling backslash

        // multi-line: backslash followed by newline/whitespace
        // means "trim all whitespace until next non-whitespace"
        if ( isMultiline && mlFnCheck( inSv[ii] ) )
        {
            while ( ii < inSv.size() && mlFnCheck( inSv[ii] ) )
                ++ii;
            --ii;
            continue;
        }

        switch (inSv[ii]) {
        case 'n':  result += '\n'; break;
        case 't':  result += '\t'; break;
        case 'r':  result += '\r'; break;
        case '"':  result += '"';  break;
        case '\\': result += '\\'; break;
        case 'b':  result += '\b'; break;
        case 'f':  result += '\f'; break;
        case 'u':
        {
            if (ii + 4 >= inSv.size()) return {};
            auto cp = std::stoul(std::string(inSv.substr(ii+1, 4)), nullptr, 16);
            result += str::EncodeUTF8(cp);
            ii += 4;
            break;
        }
        case 'U':
        {
            if (ii + 8 >= inSv.size()) return {};
            auto cp = std::stoul(std::string(inSv.substr(ii+1, 8)), nullptr, 16);
            result += str::EncodeUTF8(cp);
            ii += 8;
            break;
        }
        default: return {};  // invalid escape
        }
    }

    return result;
}

std::string asge::config::_internal::toml::ParseBasicString(std::string_view inLine, bool isLiteral) noexcept
{
    if ( inLine.size() < 2 ) return {};
    inLine = inLine.substr( 1, inLine.size() - 2 );
    if ( !isLiteral ) return ProcessEscape( inLine );
    return std::string(inLine);
}

std::string asge::config::_internal::toml::ParseMultiLineString(std::istringstream &inStream, 
    std::string_view inSv, bool isLiteral)
{
    std::string parseResult, currLine;
    std::string suffix = isLiteral ? "'''" : "\"\"\"";
    bool isEOM = false;

    // Put the first line that contains the marker inside the resulting string
    inSv.remove_prefix(3);
    parseResult = std::string( inSv );

    // Scan all the successive lines till the EOM ( End Of Multiline Marker ) is found.
    while ( std::getline( inStream, currLine ) )
    {
        std::string_view currSv = currLine;
        if ( (isEOM = currSv.ends_with(suffix) ) ) currSv.remove_suffix(3);
        if ( currSv.empty() ) return parseResult;
        if (!isLiteral) {
            parseResult += ProcessEscape( currSv, true );
        } else {
            parseResult += std::string(currSv);
        }

        if ( isEOM ) break;
        else {
            parseResult += "\n";
        }
    }

    return parseResult;
}

PathSegment asge::config::_internal::toml::ParseSegment(std::string_view inSegment)
{
    auto bracketOpen  = inSegment.find('[');
    auto bracketClose = inSegment.find(']');

    if ( bracketOpen  == std::string_view::npos ||
         bracketClose == std::string_view::npos )
        return { inSegment, std::nullopt };  // no index

    auto name  = inSegment.substr(0, bracketOpen);
    auto idxSv = inSegment.substr(bracketOpen + 1, bracketClose - bracketOpen - 1);

    return { name, static_cast<std::size_t>(std::stoul(std::string(idxSv))) };
}

std::pair<std::string_view, std::string_view>
asge::config::_internal::toml::SplitTablePath(std::string_view inTableName) noexcept
{
    auto sepPos = inTableName.rfind('.');
    if ( sepPos == std::string_view::npos ) return { {}, inTableName };
    return { inTableName.substr(0, sepPos), inTableName.substr( sepPos + 1 ) };
}

asge::Result<table_pointer> asge::config::_internal::toml::FindSubTable(
    table_pointer inParent, std::string const &inName) noexcept
{
    for ( auto const& subTable : inParent->GetSubTables() )
    {
        if ( subTable->GetName() == inName ) return Result<table_pointer>::Ok( subTable );
    }
    auto const ec = make_error_code( errors::ConfError::TomlNoSubtable );
    str::String detail = "name " + inName;
    return Result<table_pointer>::Err( ec, detail );
}

asge::Result<table_pointer> asge::config::_internal::toml::FindSubTable(
    table_const_pointer inParent, std::string const &inName) noexcept
{
    for ( auto const& subTable : inParent->GetSubTables() )
    {
        if ( subTable->GetName() == inName ) return Result<table_pointer>::Ok( subTable );
    }
    auto const ec = make_error_code( errors::ConfError::TomlNoSubtable );
    str::String detail = "name " + inName;
    return Result<table_pointer>::Err( ec, detail );
}

asge::Result<table_pointer> asge::config::_internal::toml::FindSubTableAt(table_const_pointer inParent, 
    std::string const &inName, std::size_t inIndex) noexcept
{
    std::size_t count = 0;
    for ( auto const& sub : inParent->GetSubTables() )
    {
        if ( sub->GetName() == inName )
        {
            if ( count == inIndex ) return Result<table_pointer>::Ok(sub);
            ++count;
        }
    }
    auto const ec = make_error_code( errors::ConfError::TomlNoSubtable );
    str::String detail = "name " + inName;
    return Result<table_pointer>::Err( ec, detail );
}

table_pointer asge::config::_internal::toml::FindOrCreateTable(
    table_pointer inRoot, std::string_view inTableName, TableType inType
) {
    auto currTable = inRoot;
    auto remTablePath = inTableName;

    while (!remTablePath.empty())
    {   
        // At each iteration takes one path name out of the complete path
        auto sepPos = remTablePath.find('.');
        auto currName = sepPos == std::string_view::npos
            ? remTablePath
            : remTablePath.substr( 0, sepPos );

        // Search the subtable as direct child of the current
        // table. If it does not exists than create it and add it.
        auto nextTableResult = FindSubTable( currTable, std::string( currName ) );
        if ( !nextTableResult )
        {
            auto nextTable = std::make_shared<Table>( std::string(currName), inType );
            nextTable->SetParent( currTable );
            currTable->AddSubTable( nextTable );
            currTable = nextTable;
        }
        else
        {
            currTable = nextTableResult.Value();
        }

        remTablePath = sepPos == std::string_view::npos
            ? std::string_view{}
            : remTablePath.substr( sepPos + 1 );
    }

    return currTable;
}

table_pointer asge::config::_internal::toml::ParseArrayTable(table_pointer inRoot, std::string_view inLine)
{
    // Remove the prefix and suffix marker
    inLine.remove_prefix(2);
    inLine.remove_suffix(2);
    auto tableName = str::Trim( inLine );

    // Split the table name into parent path and leaf name, such as "a.b.c"
    // becomes parent = "a.b" and leaf = "c"
    auto [ parentName, childName ] = SplitTablePath( tableName );

    // Find or create the parent in the root table if needed
    auto parent = parentName.empty()
        ? inRoot
        : FindOrCreateTable( inRoot, parentName, TableType::Standard );

    // Check if a Root table with this name already exists under parent
    // If it does, this is a subsequent Array element; 
    // otherwise it is the Root
    auto existing = FindSubTable( parent, std::string(childName) );
    auto newTable = std::make_shared<Table>( 
        std::string(childName),
        existing ? TableType::Array : TableType::Root 
    );

    newTable->SetParent(parent);
    parent->AddSubTable(newTable);
    return newTable;
}

table_pointer asge::config::_internal::toml::ParseTable(table_pointer inRoot, std::string_view inLine)
{
    inLine.remove_prefix(1);
    inLine.remove_suffix(1);
    auto tableName = str::Trim(inLine);
    auto newTable = FindOrCreateTable(inRoot, tableName, TableType::Standard);
    return newTable;
}

asge::Result<TOMLEntry> asge::config::_internal::toml::ParseValue(std::istringstream *inStream, std::string_view inLine)
{
    const auto tomlType = DetectType( inLine );

    try
    {
        switch (tomlType)
        {
        case ElementType::String:
        {
            auto strType = DetectStringType( inLine );
            auto strResult = ParseString(inStream, inLine);
            if ( !strResult ) return Result<TOMLEntry>::Err( strResult.Error() );
            return Result<TOMLEntry>::Ok(TOMLEntry{
                ValueType(std::move(strResult.Value())), TOMLTypeInfo{ ElementType::String, strType }
            });
        }
        case ElementType::Array: return ParseArray(inLine);
        case ElementType::Bool:
            return Result<TOMLEntry>::Ok(TOMLEntry{ ValueType(inLine == "true"), TOMLTypeInfo{ ElementType::Bool } });
        case ElementType::Double:
            return Result<TOMLEntry>::Ok(TOMLEntry{
                ValueType(std::stod( std::string(inLine) )), TOMLTypeInfo{ ElementType::Double }
            });
        case ElementType::Int:
            return Result<TOMLEntry>::Ok(TOMLEntry{
                ValueType(std::stoi( std::string(inLine) )), TOMLTypeInfo{ ElementType::Int }
            });
        default:
            return Result<TOMLEntry>::Err( make_error_code( errors::ConfError::TomlBadFormatting ) );
        }
        }
    catch(const std::exception& e)
    {
        return Result<TOMLEntry>::Err(make_error_code( errors::ConfError::TomlBadFormatting ), e.what() );
    }

}

asge::Result<table_pointer> asge::config::_internal::toml::Parse(std::string const &inRaw) noexcept
{
    // Creates the root table
    auto rootTable = std::make_shared<Table>( TableType::Root );
    auto currTable = rootTable;

    std::istringstream stream( inRaw );
    std::string line;
    std::string_view svLine;

    while ( std::getline(stream, line) )
    {
        // Jump to the next line if the current one is empty or it is a comment
        if ( line.empty() || line.front() == '#' ) continue;

        // Store the current line into a string_view to faster the parsing
        svLine = line;

        // Strip any possible comment from the current line
        svLine = str::Trim(StripComment( svLine ));
        if ( svLine.empty() ) continue;

        // Check if the line is a key_value pair, i.e., find the = symbol
        auto kvSep = svLine.find_first_of("=");
        if ( kvSep != std::string_view::npos )
        {
            auto kvKey    = str::Trim(svLine.substr(0, kvSep));
            auto kvValue  = str::Trim(svLine.substr(kvSep + 1, svLine.size()));
            auto kvParsed = ParseValue( &stream, kvValue );
            if ( !kvParsed )
            {
                return Result<table_pointer>::Err( kvParsed.Error() );
            }

            // Add the key-value pair into the current table
            if ( auto result = currTable->AddKvPair( std::string(kvKey), kvParsed.Value() ); !result )
            {
                return Result<table_pointer>::Err( result.Error() );
            }
            
            continue;
        }

        // If the current line is not an assignment line, then we need to
        // remove the leading and ending whitespaces.
        svLine = str::Trim( svLine );

        // Here we need to check for tables and table names. First we
        // check for table arrays which has [[ starting marker.
        if ( svLine.starts_with("[[") )
        {
            currTable = ParseArrayTable( rootTable, svLine );
            continue;
        }
        
        if ( svLine.starts_with("[") )
        {
            currTable = ParseTable( rootTable, svLine );
            continue;
        }

        // If we reach here the line is malformed
        auto const ec = make_error_code( errors::ConfError::TomlUnexpectedToken );
        return Result<table_pointer>::Err( ec, str::String(svLine) ); 
    }

    return Result<table_pointer>::Ok( std::move(rootTable) );
}

ElementType asge::config::_internal::toml::DetectType(std::string_view inLine) noexcept
{
    if (inLine.empty()) return ElementType::Null;

    if (inLine[0] == '[')                           return ElementType::Array;
    if (inLine[0] == '"' || inLine[0] == '\'')      return ElementType::String;
    if (inLine == "true")                           return ElementType::Bool;
    if (inLine == "false")                          return ElementType::Bool;
    if (inLine.find('.') != std::string_view::npos) return ElementType::Double;

    return ElementType::Int;
}

std::string_view asge::config::_internal::toml::StripComment(std::string_view inLine) noexcept
{
    bool inString = false;
    char quote    = 0;

    for ( std::size_t ii = 0; ii < inLine.size(); ++ii )
    {
        char c = inLine[ii];

        if ( !inString && ( c == '"' || c == '\'' ) ) {
            inString = true;
            quote    = c;
        } else if ( inString && c == quote && inLine[ii - 1] != '\\' ) {
            inString = false;
        } else if ( !inString && c == '#' ) {
            return inLine.substr(0, ii);  // everything before the #
        }
    }

    return inLine;
}
