#include "XML.hpp"

#include <cmath>
#include <ASGE/Core/Traits.hpp>

namespace
{

template<typename T>
asge::Result<T> ExtendResult( 
    asge::Result<T> const& inResult, asge::str::StringView inAttrName, std::uint32_t inLine
) {
    if ( !inResult )
    {
        return asge::Result<T>::Err( 
            inResult.Error(), "attribute name " + asge::str::String(inAttrName)
            + " line " + std::to_string( inLine )
        );
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
            std::make_error_code( asge::errors::ConfError::NumberParseTrailingJunk )
        );
    }

    if constexpr ( std::is_floating_point_v<T> )
    {
        if ( !std::isfinite(result) )
        {
            return asge::Result<T>::Err(
                std::make_error_code( asge::errors::ConfError::NonFiniteNumber )
            );
        }
    }

    return asge::Result<T>::Ok( result );
}

}

asge::formats::xml::XMLAttribute::XMLAttribute(
    str::StringView inName, str::StringView inValue, std::uint32_t inLine)
    : m_Value( inValue ), m_Line(inLine), m_Name( inName )
{
}

asge::Result<std::int32_t> asge::formats::xml::XMLAttribute::AsInt() const noexcept
{
    return ExtendResult(ParseNumber<std::int32_t>( m_Value ), m_Name, m_Line);
}

asge::Result<std::uint32_t> asge::formats::xml::XMLAttribute::AsUInt() const noexcept
{
    return ExtendResult(ParseNumber<std::uint32_t>( m_Value ), m_Name, m_Line);
}

asge::Result<float> asge::formats::xml::XMLAttribute::AsFloat() const noexcept
{
    return ExtendResult(ParseNumber<float>( m_Value ), m_Name, m_Line);
}

asge::Result<double> asge::formats::xml::XMLAttribute::AsDouble() const noexcept
{
    return ParseNumber<double>( m_Value );
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
    return ParseNumber<std::int64_t>( m_Value );
}

asge::Result<std::uint64_t> asge::formats::xml::XMLAttribute::AsULong() const noexcept
{
    return ParseNumber<std::uint64_t>( m_Value );
}

asge::str::StringView asge::formats::xml::XMLAttribute::Value() const noexcept
{
    return m_Value;
}

asge::Result<asge::formats::xml::XMLNode::attribute_reference> 
asge::formats::xml::XMLNode::operator[](str::String const &inAttrName) const noexcept
{
    auto it = m_Attrs.find( inAttrName );
    if ( it == m_Attrs.end())
    {
        return asge::Result<attribute_reference>::Err(
            make_error_code( errors::ConfError::XmlAttributeNotFound ),
            inAttrName
        );
    }

    return asge::Result<attribute_reference>::Ok(std::ref( it->second ));
}
