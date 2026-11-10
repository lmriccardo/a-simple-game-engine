#include "TOML_Parser.hpp"

#include <cstdio>
#include <ASGE/Core/Filesystem/FileIO.hpp>

using namespace asge::formats::toml;
using namespace asge::formats::toml::_internal;

namespace asge::formats::toml::_internal
{

namespace
{

/**
 * @brief Formats a double for TOML output.
 *
 * Appends ".0" when the text has no decimal point or exponent, so it parses
 * back as a float rather than an integer.
 */
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

/**
 * @brief Infers a plain TOMLTypeInfo from the value actually held.
 *
 * Best-effort fallback, used only when a value's TOMLTypeInfo is missing or
 * stale (e.g. an array size mismatch).
 */
TOMLTypeInfo InferTypeInfo(ValueType const& inValue) noexcept
{
    return std::visit([](auto&& v) -> TOMLTypeInfo {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) return TOMLTypeInfo{ ElementType::String, StringType::Basic };
        else if constexpr (std::is_same_v<T, bool>)    return TOMLTypeInfo{ ElementType::Bool };
        else if constexpr (std::is_same_v<T, int>)     return TOMLTypeInfo{ ElementType::Int };
        else if constexpr (std::is_same_v<T, std::int64_t>) return TOMLTypeInfo{ ElementType::Int64 };
        else if constexpr (std::is_same_v<T, double>)  return TOMLTypeInfo{ ElementType::Double };
        else return TOMLTypeInfo{ ElementType::Array };
    }, inValue);
}

/**
 * @brief Escapes inRaw so it can be written inside a TOML basic string.
 */
std::string EscapeForOutput(std::string_view inRaw, bool isMultiline) noexcept
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

/**
 * @brief Writes one value in TOML syntax, using inInfo to keep its original style.
 */
void WriteValue(std::ostream &oss, ValueType const &inValue, TOMLTypeInfo const &inInfo) noexcept
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
        else if constexpr (std::is_same_v<T, std::int64_t>)
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

/**
 * @brief Guesses the TOML kind of a value from its text (quotes, digits, brackets...).
 */
ElementType DetectType(std::string_view inLine) noexcept
{
    if (inLine.empty()) return ElementType::Null;

    if (inLine[0] == '[')                           return ElementType::Array;
    if (inLine[0] == '"' || inLine[0] == '\'')      return ElementType::String;
    if (inLine == "true")                           return ElementType::Bool;
    if (inLine == "false")                          return ElementType::Bool;
    if (inLine.find('.') != std::string_view::npos) return ElementType::Double;

    return ElementType::Int;
}

/**
 * @brief Removes a trailing `# comment` from a line, ignoring # inside quoted strings.
 */
std::string_view StripComment(std::string_view inLine) noexcept
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

/**
 * @brief Splits the inside of an array on top-level commas.
 *
 * Commas nested in inner arrays or inside strings do not split.
 */
std::vector<std::string_view> SplitArrayElements(std::string_view inStr)
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

Result<TOMLEntry> ParseValue(std::istringstream* inStream, std::string_view inLine);

/**
 * @brief Parses an `[a, b, ...]` array value, including nested arrays.
 */
asge::Result<TOMLEntry> ParseArray(std::string_view inLine)
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

/**
 * @brief Tells which of the four TOML string styles inSv starts with (or Invalid).
 */
StringType DetectStringType(std::string_view inSv) noexcept
{
    if (inSv.starts_with(R"(""")")) return StringType::MultiBasic;
    if (inSv.starts_with("'''"))    return StringType::MultiLiteral;
    if (inSv.starts_with('"'))      return StringType::Basic;
    if (inSv.starts_with('\''))     return StringType::Literal;
    return StringType::Invalid;
}

/**
 * @brief Resolves TOML escape sequences (`\n`, `\t`, `\uXXXX`, ...) in inSv.
 *
 * isMultiline additionally honors the line-ending backslash of multi-line
 * basic strings.
 */
std::string ProcessEscape(std::string_view inSv, bool isMultiline = false) noexcept
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
            result += str::EncodeUtf8(cp);
            ii += 4;
            break;
        }
        case 'U':
        {
            if (ii + 8 >= inSv.size()) return {};
            auto cp = std::stoul(std::string(inSv.substr(ii+1, 8)), nullptr, 16);
            result += str::EncodeUtf8(cp);
            ii += 8;
            break;
        }
        default: return {};  // invalid escape
        }
    }

    return result;
}

/**
 * @brief Extracts the content of a one-line basic or literal string.
 *
 * Literal strings (isLiteral) are returned raw; basic ones have their
 * escapes processed.
 */
std::string ParseBasicString(std::string_view inLine, bool isLiteral = false) noexcept
{
    if ( inLine.size() < 2 ) return {};
    inLine = inLine.substr( 1, inLine.size() - 2 );
    if ( !isLiteral ) return ProcessEscape( inLine );
    return std::string(inLine);
}

/**
 * @brief Extracts a triple-quoted string, reading more lines from inStream until it closes.
 */
std::string ParseMultiLineString(std::istringstream &inStream, 
    std::string_view inSv, bool isLiteral = false)
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

/**
 * @brief Parses a TOML string value, including empty basic and literal strings.
 */
asge::Result<asge::str::String> ParseString(
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

    if ( result.empty() && inLine != R"("")" && inLine != "''" )
    {
        auto const ec = make_error_code( errors::ConfError::TomlInvalidString );
        return Result<str::String>::Err(ec);
    }

    return Result<str::String>::Ok( std::move(result) );
}

using table_const_pointer = std::shared_ptr<Table const>;

struct PathSegment
{
    std::string_view name;             // Table name part of the segment
    std::optional<std::size_t> index;  // Array-of-tables index from `name[index]`, if any
};

/**
 * @brief Splits a path segment such as `entity[2]` into its name and optional index.
 */
PathSegment ParseSegment(std::string_view inSegment)
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

/**
 * @brief Splits a dotted table path at its last dot into (parent, leaf).
 *
 * Without a dot the parent is empty and the whole path is the leaf.
 */
std::pair<std::string_view, std::string_view>
SplitTablePath(std::string_view inTableName) noexcept
{
    auto sepPos = inTableName.rfind('.');
    if ( sepPos == std::string_view::npos ) return { {}, inTableName };
    return { inTableName.substr(0, sepPos), inTableName.substr( sepPos + 1 ) };
}

/**
 * @brief Const-parent overload of FindSubTable.
 */
asge::Result<table_pointer> FindSubTable(
    table_const_pointer inParent, std::string const &inName) noexcept
{
    // See the table_pointer overload above for why this searches back-to-front.
    auto const& subTables = inParent->GetSubTables();
    for ( auto it = subTables.rbegin(); it != subTables.rend(); ++it )
    {
        if ( (*it)->GetName() == inName ) return Result<table_pointer>::Ok( *it );
    }
    auto const ec = make_error_code( errors::ConfError::TomlNoSubtable );
    str::String detail = "name " + inName;
    return Result<table_pointer>::Err( ec, detail );
}

/**
 * @brief Finds element inIndex of the array of tables called inName.
 *
 * Returns TomlNoSubtable if the array or the index does not exist.
 */
asge::Result<table_pointer> FindSubTableAt(table_const_pointer inParent, 
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

/**
 * @brief Handles a `[[name]]` line: appends a new table to that array of tables.
 *
 * Returns the new table, so following key/value lines are added to it.
 */
table_pointer ParseArrayTable(table_pointer inRoot, std::string_view inLine)
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

/**
 * @brief Handles a `[name]` line: finds or creates that table and returns it.
 */
table_pointer ParseTable(table_pointer inRoot, std::string_view inLine)
{
    inLine.remove_prefix(1);
    inLine.remove_suffix(1);
    auto tableName = str::Trim(inLine);
    auto newTable = FindOrCreateTable(inRoot, tableName, TableType::Standard);
    return newTable;
}

/**
 * @brief Parses the value text of a `key = value` line into an entry.
 *
 * inStream is only needed for multi-line strings, which continue on the
 * following lines; pass nullptr when parsing a single array element.
 * Returns an error Result on malformed text.
 */
asge::Result<TOMLEntry> ParseValue(std::istringstream *inStream, std::string_view inLine)
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
        {
            std::string const numStr(inLine);
            try
            {
                return Result<TOMLEntry>::Ok(TOMLEntry{
                    ValueType(std::stoi( numStr )), TOMLTypeInfo{ ElementType::Int }
                });
            }
            catch ( std::out_of_range const& )
            {
                // Doesn't fit in a 32-bit int (e.g. a packed 64-bit color) --
                // widen to Int64 instead of failing the whole parse. A
                // genuinely malformed token throws invalid_argument instead,
                // which isn't caught here and falls through to this
                // function's own outer catch, same as before.
                return Result<TOMLEntry>::Ok(TOMLEntry{
                    ValueType(static_cast<std::int64_t>( std::stoll( numStr ) )), TOMLTypeInfo{ ElementType::Int64 }
                });
            }
        }
        default:
            return Result<TOMLEntry>::Err( make_error_code( errors::ConfError::TomlBadFormatting ) );
        }
        }
    catch(const std::exception& e)
    {
        return Result<TOMLEntry>::Err(make_error_code( errors::ConfError::TomlBadFormatting ), e.what() );
    }

}

}

}

std::string asge::formats::toml::_internal::Table::GetAbsoluteName() const noexcept
{
    if ( auto pTable = m_ParentTable.lock() )
    {
        auto pName = pTable->GetAbsoluteName();
        if ( pName.empty() ) return m_Name;
        return pName + "." + m_Name;
    }

    return m_Name;
}

void asge::formats::toml::_internal::Table::PrintTable(std::ostream &oss) const noexcept
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

asge::BoolResult asge::formats::toml::_internal::Table::AddKvPair(std::string const &inKey, TOMLEntry inEntry) noexcept
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

void asge::formats::toml::_internal::Table::AddSubTable(Table::pointer inChild) noexcept
{
    m_SubTables.push_back( inChild );
    inChild->SetParent( shared_from_this() );
}

void asge::formats::toml::_internal::Table::SetParent(std::weak_ptr<Table> inParent) noexcept
{
    m_ParentTable.swap( inParent );
}

std::vector<Table::pointer> const &asge::formats::toml::_internal::Table::GetSubTables() const noexcept
{
    return m_SubTables;
}

std::string const &asge::formats::toml::_internal::Table::GetName() const noexcept
{
    return m_Name;
}

asge::Result<Table::pointer> asge::formats::toml::_internal::Table::GetTable(std::string const &inPath) const noexcept
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

asge::Result<TOMLEntry*> asge::formats::toml::_internal::Table::FindEntry(std::string const &inPath) noexcept
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

asge::Result<table_pointer> asge::formats::toml::_internal::FindSubTable(
    table_pointer inParent, std::string const &inName) noexcept
{
    // Search back-to-front: when inName names an array-of-tables, several
    // subtables share it, and an unindexed lookup must resolve to the most
    // recently defined element per TOML semantics (e.g. a bare [a.b] header
    // right after a run of [[a]] blocks belongs to the last one).
    auto const& subTables = inParent->GetSubTables();
    for ( auto it = subTables.rbegin(); it != subTables.rend(); ++it )
    {
        if ( (*it)->GetName() == inName ) return Result<table_pointer>::Ok( *it );
    }
    auto const ec = make_error_code( errors::ConfError::TomlNoSubtable );
    str::String detail = "name " + inName;
    return Result<table_pointer>::Err( ec, detail );
}

table_pointer asge::formats::toml::_internal::FindOrCreateTable(
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

asge::Result<table_pointer> asge::formats::toml::Parse(std::string const &inRaw) noexcept
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

asge::Result<table_pointer> asge::formats::toml::Parse(filesystem::Path const &inPath) noexcept
{
    auto readResult = filesystem::ReadText( inPath );
    if ( !readResult ) return Result<table_pointer>::Err( readResult.Error() );

    return Parse( readResult.Value() );
}
