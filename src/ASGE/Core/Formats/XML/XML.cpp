#include "XML.hpp"

#include <cmath>
#include <array>
#include <algorithm>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <fstream>
#include <sstream>
#include <ASGE/Core/Traits.hpp>

namespace
{

template<typename T, typename ... Args>
asge::Result<T> EnhanceErrorWith( asge::Result<T> const& inResult, Args&&... inArgs )
{
    if ( !inResult )
    {
        std::ostringstream ss;
        ss << inResult.Error().s_Detail << " ";
        (ss << ... << std::forward<Args>(inArgs));
        return asge::Result<T>::Err( inResult.Error().s_Code, ss.str() );
    }

    return inResult;
}

template<asge::_internal::traits::ParsableNumber T>
asge::Result<T> ParseNumber( asge::str::StringView inText ) noexcept
{
    T result{};
    char const* const first = inText.data();
    char const* const last  = first + inText.size();

    auto [ptr, ec] = std::from_chars( first, last, result );

    if ( ec != std::errc{} )
    {
        return asge::Result<T>::Err( std::make_error_code(ec) );
    }

    if ( ptr != last )
    {
        return asge::Result<T>::Err(
            make_error_code( asge::errors::ConfError::NumberParseTrailingJunk )
        );
    }

    if constexpr ( std::is_floating_point_v<T> )
    {
        if ( !std::isfinite(result) )
        {
            return asge::Result<T>::Err(
                make_error_code( asge::errors::ConfError::NonFiniteNumber )
            );
        }
    }

    return asge::Result<T>::Ok( result );
}

static constexpr unsigned k_MaxDepth = 64;

asge::BoolResult ParseClosingTag( 
    asge::formats::xml::XMLNode const& inNode, asge::str::StringView inLine ) noexcept
{
    inLine.remove_prefix(2);
    asge::str::StringView name = inNode.Name();
    if ( inLine.find( name ) == asge::str::StringView::npos )
    {
        return asge::BoolResult::Err(
            make_error_code( asge::errors::ConfError::XmlNonMatchingClosingTag ),
            inNode.Name()
        );
    }

    inLine.remove_prefix(name.size());
    if ( inLine.find(">") == asge::str::StringView::npos )
    {
        return asge::BoolResult::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat )
        );
    }

    return asge::BoolResult::Ok();
}

asge::Result<std::size_t> ParseNodeAttributes( 
    asge::formats::xml::XMLNode& outNode, asge::str::StringView inLine, std::uint32_t inLineNo
) noexcept
{
    std::size_t eqPos = inLine.find("=");
    if ( eqPos == 0 )
    {
        return asge::Result<std::size_t>::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "missing attribute name at line " + std::to_string(inLineNo)
        );
    }

    if ( eqPos == asge::str::StringView::npos )
    {
        return asge::Result<std::size_t>::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "missing equal sign at line " + std::to_string(inLineNo)
        );
    }

    asge::str::StringView attrName = asge::str::Trim(inLine.substr( 0, eqPos ));
    std::size_t openDQPos = inLine.find("\"", eqPos + 1);
    std::size_t closDQPos = inLine.find("\"", openDQPos + 1);

    if ( openDQPos == asge::str::StringView::npos )
    {
        return asge::Result<std::size_t>::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "missing open double quotes at line " + std::to_string(inLineNo)
        );
    }

    if ( closDQPos == asge::str::StringView::npos )
    {
        return asge::Result<std::size_t>::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "missing closing double quotes at line " + std::to_string(inLineNo)
        );
    }

    if ( openDQPos + 1 == closDQPos )
    {
        return asge::Result<std::size_t>::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "missing attribute value at line " + std::to_string(inLineNo)
        );
    }

    asge::str::StringView attrValue = inLine.substr( openDQPos + 1, closDQPos - openDQPos - 1 );
    if ( auto result = outNode.AddAttribute( attrName, attrValue ); !result )
    {
        return asge::Result<std::size_t>::Err(
            result.Error().s_Code,
            result.Error().s_Detail + " at line " + std::to_string( inLineNo )
        );
    }

    return asge::Result<std::size_t>::Ok( closDQPos + 1 );
}

asge::BoolResult ParseXMLNode( asge::formats::xml::XMLNode& outNode, 
                               asge::str::StringView inLine,
                               std::uint32_t inLineNo,
                               bool& outAutoClosing ) noexcept
{
    outAutoClosing = inLine.ends_with( "/>" );
    inLine.remove_prefix( 1 );
    inLine = asge::str::Trim( inLine );

    std::size_t spacePos = inLine.find( " " );
    if ( spacePos == 0 )
    {
        // Once I have trimmed the leading and trailing whitespaces
        // there should be no whitespace left before the tag name
        return asge::BoolResult::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "missing tag name on line " + std::to_string( inLineNo )
        );
    }

    // Search for the closing bracket on tags
    std::size_t endPos{};
    if ( endPos = inLine.find("/>"); endPos == asge::str::StringView::npos )
    {
        if ( endPos = inLine.find(">"); endPos == asge::str::StringView::npos )
        {
            return asge::BoolResult::Err(
                make_error_code( asge::errors::ConfError::XmlBadFormat ),
                "missing closing bracket on line " + std::to_string( inLineNo )
            );
        }
    }

    // Take the middle content between the first letter and the closing position
    inLine = asge::str::Trim( inLine.substr( 0, endPos ) );

    // Name runs up to the first whitespace, attributes follow one after another
    std::size_t nameEnd = inLine.find_first_of( " \t" );
    outNode.SetName( inLine.substr( 0, nameEnd ), inLineNo );
    inLine = nameEnd == asge::str::StringView::npos ? asge::str::StringView{} : inLine.substr( nameEnd );

    while ( !( inLine = asge::str::Trim( inLine ) ).empty() )
    {
        auto result = ParseNodeAttributes( outNode, inLine, inLineNo );
        if ( !result )
        {
            return asge::BoolResult::Err( result.Error() );
        }

        inLine.remove_prefix( result.Value() );
    }

    return asge::BoolResult::Ok();
}

struct OpenElements
{
    std::array<asge::formats::xml::XMLNode*, k_MaxDepth> m_Nodes {}; // Open elements, root first
    std::size_t m_Depth    { 0 };     // Number of open elements
    bool        m_RootDone { false }; // Root element already closed
};

asge::BoolResult OpenElement( OpenElements& ioOpen,
                              asge::formats::xml::XMLNode& outRoot,
                              asge::str::StringView inLine,
                              std::uint32_t inLineNo ) noexcept
{
    asge::formats::xml::XMLNode* node = &outRoot;
    if ( ioOpen.m_Depth > 0 )
    {
        node = &ioOpen.m_Nodes[ioOpen.m_Depth - 1]->AddChild();
    }
    else if ( ioOpen.m_RootDone )
    {
        return asge::BoolResult::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "multiple root elements at line " + std::to_string( inLineNo )
        );
    }

    bool autoClosing { false };
    if ( auto result = ParseXMLNode( *node, inLine, inLineNo, autoClosing ); !result )
    {
        return result;
    }

    if ( autoClosing )
    {
        ioOpen.m_RootDone = ioOpen.m_RootDone || ioOpen.m_Depth == 0;
        return asge::BoolResult::Ok();
    }

    if ( ioOpen.m_Depth == k_MaxDepth )
    {
        return asge::BoolResult::Err(
            make_error_code( asge::errors::ConfError::XmlNestingTooDeep ),
            "Line " + std::to_string( inLineNo )
        );
    }

    ioOpen.m_Nodes[ioOpen.m_Depth++] = node;
    return asge::BoolResult::Ok();
}

asge::BoolResult CloseElement( OpenElements& ioOpen,
                               asge::str::StringView inLine,
                               std::uint32_t inLineNo ) noexcept
{
    if ( ioOpen.m_Depth == 0 )
    {
        return asge::BoolResult::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "unexpected closing tag at line " + std::to_string( inLineNo )
        );
    }

    if ( auto result = ParseClosingTag( *ioOpen.m_Nodes[ioOpen.m_Depth - 1], inLine ); !result )
    {
        return asge::BoolResult::Err( result.Error().s_Code,
            result.Error().s_Detail + " Line " + std::to_string( inLineNo )
        );
    }

    ioOpen.m_RootDone = --ioOpen.m_Depth == 0;
    return asge::BoolResult::Ok();
}

asge::BoolResult ParseXMLElement(
    asge::formats::xml::XMLNode& outNode, std::fstream& inFhandle ) noexcept
{
    OpenElements  open;
    std::uint32_t currLine   { 0 };
    bool          xmlContent { false };

    asge::str::String currLineStr;

    for ( ; std::getline( inFhandle, currLineStr ); ++currLine )
    {
        // Creates the string view representation of the current line and
        // immediately trims leading and trailing white spaces
        asge::str::StringView line = asge::str::Trim( currLineStr );
        if ( line.empty() || line.starts_with( "<!--" ) )
        {
            xmlContent = line.starts_with( "<!--" );
            continue;
        }

        // Check for the XML declaration on the top of the XML file
        if ( line.starts_with( "<?xml " ) )
        {
            if ( (currLine > 0 && xmlContent) || !line.ends_with( "?>" ) )
            {
                return asge::BoolResult::Err(
                    make_error_code( asge::errors::ConfError::XmlBadFormat ),
                    " Line " + std::to_string( currLine )
                );
            }

            xmlContent = true;
            continue;
        }

        // Find inline comments
        if ( auto cPos = line.find( "<!" ); cPos != asge::str::StringView::npos )
        {
            line = asge::str::Trim( line.substr( 0, cPos ) );
        }

        asge::BoolResult result = asge::BoolResult::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "Line " + std::to_string( currLine )
        );

        if ( line.starts_with( "</" ) )     result = CloseElement( open, line, currLine );
        else if ( line.starts_with( "<" ) ) result = OpenElement( open, outNode, line, currLine );

        if ( !result ) return result;
        xmlContent = true;
    }

    if ( open.m_Depth != 0 )
    {
        return asge::BoolResult::Err(
            make_error_code( asge::errors::ConfError::XmlBadFormat ),
            "unclosed tag " + open.m_Nodes[open.m_Depth - 1]->Name()
        );
    }

    return asge::BoolResult::Ok();
}

}

std::size_t asge::formats::xml::TransparentHash::operator()(str::StringView inK) const noexcept
{
    return std::hash<str::StringView>{}(inK);
}

asge::formats::xml::XMLAttribute::XMLAttribute(
    str::StringView inName, str::StringView inValue, std::uint32_t inLine)
    : m_Value( inValue ), m_Line(inLine), m_Name( inName )
{
}

asge::Result<std::int32_t> asge::formats::xml::XMLAttribute::AsInt() const noexcept
{
    return EnhanceErrorWith(ParseNumber<std::int32_t>( m_Value ), 
        "attribute name ", m_Name, " line ", m_Line
    );
}

asge::Result<std::uint32_t> asge::formats::xml::XMLAttribute::AsUInt() const noexcept
{
    return EnhanceErrorWith(ParseNumber<std::uint32_t>( m_Value ),
        "attribute name ", m_Name, " line ", m_Line
    );
}

asge::Result<float> asge::formats::xml::XMLAttribute::AsFloat() const noexcept
{
    return EnhanceErrorWith(ParseNumber<float>( m_Value ), 
        "attribute name ", m_Name, " line ", m_Line
    );
}

asge::Result<double> asge::formats::xml::XMLAttribute::AsDouble() const noexcept
{
    return EnhanceErrorWith(ParseNumber<double>( m_Value ), 
        "attribute name ", m_Name, " line ", m_Line
    );
}

asge::Result<bool> asge::formats::xml::XMLAttribute::AsBool() const noexcept
{
    if ( m_Value != "true" && m_Value != "false" )
    {
        return asge::Result<bool>::Err( std::make_error_code(std::errc::invalid_argument) );
    }

    return asge::Result<bool>::Ok(m_Value == "true");
}

asge::Result<std::int64_t> asge::formats::xml::XMLAttribute::AsLong() const noexcept
{
    return EnhanceErrorWith(ParseNumber<std::int64_t>( m_Value ), 
        "attribute name ", m_Name, " line ", m_Line
    );
}

asge::Result<std::uint64_t> asge::formats::xml::XMLAttribute::AsULong() const noexcept
{
    return EnhanceErrorWith(ParseNumber<std::uint64_t>( m_Value ), 
        "attribute name ", m_Name, " line ", m_Line
    );
}

asge::str::StringView asge::formats::xml::XMLAttribute::Value() const noexcept
{
    return m_Value;
}

asge::formats::xml::XMLNode::XMLNode(str::StringView inName, std::uint32_t inLine)
    : m_Name( inName ), m_Line( inLine )
{
}

void asge::formats::xml::XMLNode::SetName(str::StringView inName, std::uint32_t inLine)
{
    m_Name = inName;
    m_Line = inLine;
}

asge::BoolResult asge::formats::xml::XMLNode::AddAttribute(
    str::StringView inAttrName, str::StringView inAttrValue)
{
    auto attr = XMLAttribute( inAttrName, inAttrValue, m_Line );
    auto [_, result] = m_Attrs.emplace( inAttrName, std::move(attr) );
    if ( !result )
    {
        return BoolResult::Err( 
            make_error_code( errors::ConfError::XmlDuplicatedAttribute ), 
            str::String(inAttrName) 
        );
    }

    return BoolResult::Ok();
}

asge::formats::xml::XMLNode &asge::formats::xml::XMLNode::AddChild()
{
    return m_Children.emplace_back();
}

asge::Result<asge::formats::xml::XMLNode::attribute_reference>
asge::formats::xml::XMLNode::operator[](str::StringView inAttrName) const noexcept
{
    auto it = m_Attrs.find( inAttrName );
    if ( it == m_Attrs.end())
    {
        return asge::Result<attribute_reference>::Err(
            make_error_code( errors::ConfError::XmlAttributeNotFound ),
            str::String(inAttrName)
        );
    }

    return asge::Result<attribute_reference>::Ok(std::ref( it->second ));
}

asge::formats::xml::XMLAttribute const *
asge::formats::xml::XMLNode::Find(str::StringView inAttrName) const noexcept
{
    auto it = m_Attrs.find( inAttrName );
    if ( it == m_Attrs.end() ) return nullptr;
    return &it->second;
}

asge::str::String const &asge::formats::xml::XMLNode::Name() const noexcept
{
    return m_Name;
}

asge::Result<asge::formats::xml::XMLNode::node_reference>
asge::formats::xml::XMLNode::Child(std::size_t inChildIdx) const noexcept
{
    if ( inChildIdx >= m_Children.size() )
    {
        return Result<node_reference>::Err(
            std::make_error_code( std::errc::invalid_argument ),
            "not enough children for node " + m_Name
        );
    }

    return Result<node_reference>::Ok( std::ref( m_Children.at( inChildIdx) ) );
}

std::vector<asge::formats::xml::XMLNode>::iterator 
asge::formats::xml::XMLNode::begin() noexcept
{
    return m_Children.begin();
}

std::vector<asge::formats::xml::XMLNode>::const_iterator 
asge::formats::xml::XMLNode::begin() const noexcept
{
    return cbegin();
}

std::vector<asge::formats::xml::XMLNode>::iterator 
asge::formats::xml::XMLNode::end() noexcept
{
    return m_Children.end();
}

std::vector<asge::formats::xml::XMLNode>::const_iterator 
asge::formats::xml::XMLNode::end() const noexcept
{
    return cend();
}

std::vector<asge::formats::xml::XMLNode>::const_iterator 
asge::formats::xml::XMLNode::cbegin() const noexcept
{
    return m_Children.cbegin();
}

std::vector<asge::formats::xml::XMLNode>::const_iterator 
asge::formats::xml::XMLNode::cend() const noexcept
{
    return m_Children.cend();
}

asge::BoolResult asge::formats::xml::XMLNode::Serialize(
    str::String& outText, unsigned inDepth) const
{
    if ( inDepth >= k_MaxDepth )
    {
        return BoolResult::Err( make_error_code( errors::ConfError::XmlNestingTooDeep ), m_Name );
    }

    auto const badFormat = [this]( str::String inDetail ) {
        return BoolResult::Err(
            make_error_code( errors::ConfError::XmlBadFormat ), m_Name + ": " + inDetail
        );
    };

    auto const badName = []( str::StringView inName ) {
        return inName.empty() || inName.find_first_of( " \t\r\n<>/=\"" ) != str::StringView::npos;
    };

    if ( badName( m_Name ) ) return badFormat( "invalid element name" );

    std::vector<XMLAttribute const*> attrs;
    for ( auto const& [_, attr] : m_Attrs ) attrs.push_back( &attr );
    std::ranges::sort( attrs, {}, &XMLAttribute::m_Name );

    str::String const indent( inDepth * 2, ' ' );
    outText += indent + "<" + m_Name;
    for ( auto const* attr : attrs )
    {
        auto const value = attr->Value();
        if ( badName( attr->m_Name ) ) return badFormat( "invalid attribute name " + attr->m_Name );
        if ( value.empty() || value.find_first_of( "\"<>\r\n" ) != str::StringView::npos )
        {
            return badFormat( "unsupported value for attribute " + attr->m_Name );
        }

        outText += " " + attr->m_Name + "=\"" + str::String( value ) + "\"";
    }

    if ( m_Children.empty() )
    {
        outText += "/>\n";
        return BoolResult::Ok();
    }

    outText += ">\n";
    for ( auto const& child : m_Children )
    {
        if ( auto result = child.Serialize( outText, inDepth + 1 ); !result ) return result;
    }

    outText += indent + "</" + m_Name + ">\n";
    return BoolResult::Ok();
}

asge::Result<asge::str::String> asge::formats::xml::XMLNode::ToString() const
{
    str::String text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    if ( auto result = Serialize( text, 0 ); !result )
    {
        return Result<str::String>::Err( result.Error() );
    }

    return Result<str::String>::Ok( std::move( text ) );
}

asge::BoolResult asge::formats::xml::XMLNode::SaveToFile(filesystem::Path const &inPath) const
{
    auto text = ToString();
    if ( !text ) return BoolResult::Err( text.Error() );
    return filesystem::WriteText( inPath, text.Value() );
}

asge::Result<asge::formats::xml::XMLNode>
asge::formats::xml::Parse(filesystem::Path const &inPath) noexcept
{
    std::fstream handle( inPath );
    if ( !handle.is_open() )
    {
        auto ec = std::make_error_code( std::errc::no_such_file_or_directory );
        return asge::Result<xml::XMLNode>::Err( ec, inPath.string() ); 
    }

    XMLNode rootNode;
    if (auto result = ParseXMLElement( rootNode, handle ); !result)
    {
        return Result<XMLNode>::Err( result.Error() );
    }

    return Result<XMLNode>::Ok( std::move(rootNode) );
}
