#pragma once

#include <optional>
#include <vector>

#include <ASGE/Core/Formats/XML/XML.hpp>
#include "AnimStateMachine.hpp"

namespace asge::game::asset::_internal
{

/**
 * @brief Builds an error Result with the given code and detail message.
 *
 * Shorthand used by the animation XML loaders so every failure carries the
 * element and attribute it came from.
 */
template<typename T>
Result<T> Fail( errors::AnimStateMachineErrors inCode, str::String inDetail )
{
    return Result<T>::Err( make_error_code( inCode ), std::move( inDetail ) );
}

/**
 * @brief Reads a mandatory attribute and returns its trimmed value.
 *
 * Fails with MissingAttribute when absent and EmptyAttribute when the value
 * is blank. inCtx names the element in the error detail. The view points into
 * the node and is valid as long as it is.
 */
Result<str::StringView> RequiredAttr( formats::xml::XMLNode const& inNode,
                                      str::StringView inName, str::String const& inCtx );

/**
 * @brief Reads an optional attribute and returns its trimmed value, if present.
 *
 * A missing attribute is fine (nullopt), but a present one must not be blank:
 * that fails with EmptyAttribute.
 */
Result<std::optional<str::StringView>> OptionalAttr( formats::xml::XMLNode const& inNode,
                                                     str::StringView inName, str::String const& inCtx );

/**
 * @brief Reads an optional attribute through inConvert, or returns inDefault when absent.
 *
 * inConvert receives the XMLAttribute and returns a Result (e.g. AsFloat).
 * A blank value fails with EmptyAttribute, a rejected one with InvalidAttribute.
 */
template<typename T, typename Convert>
Result<T> ConvertedAttr( formats::xml::XMLNode const& inNode, str::StringView inName,
                         str::String const& inCtx, T inDefault, Convert inConvert )
{
    auto value = OptionalAttr( inNode, inName, inCtx );
    if ( !value ) return Result<T>::Err( value.Error() );
    if ( !value.Value() ) return Result<T>::Ok( inDefault );

    auto converted = inConvert( *inNode.Find( inName ) );
    if ( !converted )
    {
        return Fail<T>( errors::AnimStateMachineErrors::InvalidAttribute,
            inCtx + " attribute '" + str::String( inName ) + "' has invalid value '"
            + str::String( *value.Value() ) + "'" );
    }

    return Result<T>::Ok( converted.Value() );
}

/**
 * @brief Returns the position of inName in inNames, or -1.
 */
int IndexOf( std::vector<str::String> const& inNames, str::StringView inName ) noexcept;

/**
 * @brief Appends a clip, flag, trigger or state name after validating it.
 *
 * The name must not contain whitespace or start with '!' (names are written
 * bare in `when`), must be unique in ioNames, and the list must stay under
 * inLimit. inKind names the kind of entry in the error detail.
 */
BoolResult AddName( std::vector<str::String>& ioNames, str::StringView inName,
                    str::StringView inKind, std::size_t inLimit );

}
