#pragma once

#include <cstdint>
#include <unordered_map>
#include <functional>
#include <memory>

#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Errors.hpp>

namespace asge::formats::xml
{

class XMLAttribute
{
    str::String   m_Value;  // Its string represented value
    std::uint32_t m_Line;   // Line number in the XML file
public:
    str::String m_Name;     // The name of the XML attribute

    XMLAttribute( str::StringView inName, str::StringView inValue, std::uint32_t inLine );

    [[nodiscard]] Result<std::int32_t>  AsInt     () const noexcept;
    [[nodiscard]] Result<std::uint32_t> AsUInt    () const noexcept;
    [[nodiscard]] Result<float>         AsFloat   () const noexcept;
    [[nodiscard]] Result<double>        AsDouble  () const noexcept;
    [[nodiscard]] Result<bool>          AsBool    () const noexcept;
    [[nodiscard]] Result<std::int64_t>  AsLong    () const noexcept;
    [[nodiscard]] Result<std::uint64_t> AsULong   () const noexcept;
    
    [[nodiscard]] str::StringView Value() const noexcept;
};

class XMLNode
{
    str::String m_Name; // The name of the XML Node
    std::unordered_map<str::String, XMLAttribute> m_Attrs; // A vector of XML attributes
    std::vector<XMLNode> m_Children;
    std::uint32_t m_Line;

public:
    using attribute_reference = std::reference_wrapper<XMLAttribute const>;

    Result<attribute_reference> operator[]( str::String const& inAttrName ) const noexcept;
};

}