#include "AnimXml.hpp"

#include <algorithm>

namespace
{

constexpr char const* k_Whitespace = " \t\r\n";

}

namespace asge::game::asset::_internal
{

Result<str::StringView> RequiredAttr( formats::xml::XMLNode const& inNode,
                                      str::StringView inName, str::String const& inCtx )
{
    auto const* attr = inNode.Find( inName );
    if ( !attr )
    {
        return Fail<str::StringView>( errors::AnimStateMachineErrors::MissingAttribute,
            inCtx + " requires attribute '" + str::String( inName ) + "'" );
    }

    auto const value = str::Trim( attr->Value() );
    if ( value.empty() )
    {
        return Fail<str::StringView>( errors::AnimStateMachineErrors::EmptyAttribute,
            inCtx + " has an empty attribute '" + str::String( inName ) + "'" );
    }

    return Result<str::StringView>::Ok( value );
}

Result<std::optional<str::StringView>> OptionalAttr( formats::xml::XMLNode const& inNode,
                                                     str::StringView inName, str::String const& inCtx )
{
    using Out = std::optional<str::StringView>;

    auto const* attr = inNode.Find( inName );
    if ( !attr ) return Result<Out>::Ok( std::nullopt );

    auto const value = str::Trim( attr->Value() );
    if ( value.empty() )
    {
        return Fail<Out>( errors::AnimStateMachineErrors::EmptyAttribute,
            inCtx + " has an empty attribute '" + str::String( inName ) + "'" );
    }

    return Result<Out>::Ok( value );
}

int IndexOf( std::vector<str::String> const& inNames, str::StringView inName ) noexcept
{
    auto it = std::ranges::find( inNames, inName );
    return it == inNames.end() ? -1 : static_cast<int>( it - inNames.begin() );
}

BoolResult AddName( std::vector<str::String>& ioNames, str::StringView inName,
                    str::StringView inKind, std::size_t inLimit )
{
    auto const bad = [&]( errors::AnimStateMachineErrors inCode, str::String inWhy ) {
        return BoolResult::Err( make_error_code( inCode ),
            str::String( inKind ) + " '" + str::String( inName ) + "' " + inWhy );
    };

    if ( inName.find_first_of( k_Whitespace ) != str::StringView::npos || inName.starts_with( '!' ) )
    {
        return bad( errors::AnimStateMachineErrors::InvalidAttribute,
            "cannot contain whitespace or start with '!'" );
    }

    if ( IndexOf( ioNames, inName ) >= 0 )
    {
        return bad( errors::AnimStateMachineErrors::DuplicateName, "is declared twice" );
    }

    if ( ioNames.size() >= inLimit )
    {
        return bad( errors::AnimStateMachineErrors::LimitExceeded,
            "exceeds the limit of " + std::to_string( inLimit ) );
    }

    ioNames.emplace_back( inName );
    return BoolResult::Ok();
}

}
