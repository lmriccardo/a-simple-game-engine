#include "AnimStateMachine.hpp"

#include <algorithm>
#include <functional>

#include "AnimXml.hpp"

namespace
{

using asge::errors::AnimStateMachineErrors;
using asge::formats::xml::XMLNode;
using asge::game::asset::AnimationStateMachine;
using asge::game::asset::EndMode;
using asge::str::String;
using asge::str::StringView;
using namespace asge::game::asset::_internal;

constexpr char const* k_Whitespace = " \t\r\n";

/**
 * @brief Reads a `<Flags>`, `<Triggers>` or `<Clips>` list into ioNames.
 *
 * Every child must be an inItem element with a non-empty `name`.
 */
asge::BoolResult ParseNameList( XMLNode const& inList, StringView inItem,
                                std::vector<String>& ioNames, std::size_t inLimit )
{
    String const listCtx = "<" + inList.Name() + ">";
    for ( auto const& child : inList )
    {
        if ( child.Name() != inItem )
        {
            return asge::BoolResult::Err( make_error_code( AnimStateMachineErrors::UnexpectedElement ),
                listCtx + " can only contain <" + String( inItem ) + ">, found <" + child.Name() + ">" );
        }

        auto name = RequiredAttr( child, "name", "<" + String( inItem ) + ">" );
        if ( !name ) return asge::BoolResult::Err( name.Error() );

        if ( auto added = AddName( ioNames, name.Value(), inItem, inLimit ); !added ) return added;
    }

    return asge::BoolResult::Ok();
}

/**
 * @brief Reads the `end` attribute of a state; defaults to Loop.
 */
asge::Result<EndMode> ParseEndMode( XMLNode const& inState, String const& inCtx )
{
    auto value = OptionalAttr( inState, "end", inCtx );
    if ( !value ) return asge::Result<EndMode>::Err( value.Error() );
    if ( !value.Value() ) return asge::Result<EndMode>::Ok( EndMode::Loop );

    auto const text = *value.Value();
    if ( text == "loop" )    return asge::Result<EndMode>::Ok( EndMode::Loop );
    if ( text == "hold" )    return asge::Result<EndMode>::Ok( EndMode::Hold );
    if ( text == "restore" ) return asge::Result<EndMode>::Ok( EndMode::Restore );

    return Fail<EndMode>( AnimStateMachineErrors::InvalidAttribute,
        inCtx + " attribute 'end' must be loop, hold or restore, got '" + String( text ) + "'" );
}

/**
 * @brief Translates a `when` expression such as `running !on_ground` into flag masks.
 *
 * Plain names must be set, `!name` must be clear. An unknown flag fails with
 * FlagNotFound; a flag required both set and clear, or a bare '!', with
 * InvalidAttribute.
 */
asge::BoolResult ParseWhen( StringView inWhen, std::vector<String> const& inFlags,
                            String const& inCtx, std::uint32_t& outSet, std::uint32_t& outClear )
{
    while ( true )
    {
        auto start = inWhen.find_first_not_of( k_Whitespace );
        if ( start == StringView::npos ) break;
        inWhen.remove_prefix( start );

        auto end = inWhen.find_first_of( k_Whitespace );
        auto token = inWhen.substr( 0, end );
        inWhen = end == StringView::npos ? StringView{} : inWhen.substr( end );

        bool const negated = token.starts_with( '!' );
        auto const name = negated ? token.substr( 1 ) : token;
        if ( name.empty() )
        {
            return asge::BoolResult::Err( make_error_code( AnimStateMachineErrors::InvalidAttribute ),
                inCtx + " has a '!' without a flag name in 'when'" );
        }

        int const idx = IndexOf( inFlags, name );
        if ( idx < 0 )
        {
            return asge::BoolResult::Err( make_error_code( AnimStateMachineErrors::FlagNotFound ),
                inCtx + " uses unknown flag '" + String( name ) + "'" );
        }

        ( negated ? outClear : outSet ) |= ( 1u << idx );
    }

    if ( outSet & outClear )
    {
        return asge::BoolResult::Err( make_error_code( AnimStateMachineErrors::InvalidAttribute ),
            inCtx + " requires a flag to be both set and clear in 'when'" );
    }

    return asge::BoolResult::Ok();
}

/**
 * @brief A transition together with its sort key, before the CSR layout.
 */
struct PendingTransition
{
    AnimationStateMachine::Transition m_Transition {}; // The transition itself
    int                               m_Priority   {}; // Higher fires first
};

/**
 * @brief Reads one `<Transition>` element of a state.
 *
 * `to` is required and must name a state; `trigger`, `when`, `atEnd` and
 * `priority` are optional but, when present, must be non-blank and valid.
 */
asge::Result<PendingTransition> ParseTransition(
    XMLNode const& inNode, StringView inFrom, std::vector<String> const& inStates,
    std::vector<String> const& inFlags, std::vector<String> const& inTriggers )
{
    String const ctx = "<Transition> in state '" + String( inFrom ) + "'";
    PendingTransition out;

    auto to = RequiredAttr( inNode, "to", ctx );
    if ( !to ) return asge::Result<PendingTransition>::Err( to.Error() );

    int const toIdx = IndexOf( inStates, to.Value() );
    if ( toIdx < 0 )
    {
        return Fail<PendingTransition>( AnimStateMachineErrors::StateNotFound,
            ctx + " targets unknown state '" + String( to.Value() ) + "'" );
    }
    out.m_Transition.m_To = static_cast<std::uint16_t>( toIdx );

    auto trigger = OptionalAttr( inNode, "trigger", ctx );
    if ( !trigger ) return asge::Result<PendingTransition>::Err( trigger.Error() );
    if ( trigger.Value() )
    {
        int const trigIdx = IndexOf( inTriggers, *trigger.Value() );
        if ( trigIdx < 0 )
        {
            return Fail<PendingTransition>( AnimStateMachineErrors::TriggerNotFound,
                ctx + " uses unknown trigger '" + String( *trigger.Value() ) + "'" );
        }
        out.m_Transition.m_Trigger = 1u << trigIdx;
    }

    auto when = OptionalAttr( inNode, "when", ctx );
    if ( !when ) return asge::Result<PendingTransition>::Err( when.Error() );
    if ( when.Value() )
    {
        auto parsed = ParseWhen( *when.Value(), inFlags, ctx,
            out.m_Transition.m_FlagsSet, out.m_Transition.m_FlagsClear );
        if ( !parsed ) return asge::Result<PendingTransition>::Err( parsed.Error() );
    }

    auto atEnd = ConvertedAttr<bool>( inNode, "atEnd", ctx, false,
        []( auto const& inAttr ) { return inAttr.AsBool(); } );
    if ( !atEnd ) return asge::Result<PendingTransition>::Err( atEnd.Error() );
    out.m_Transition.m_AtEnd = atEnd.Value();

    auto priority = ConvertedAttr<int>( inNode, "priority", ctx, 0,
        []( auto const& inAttr ) { return inAttr.AsInt(); } );
    if ( !priority ) return asge::Result<PendingTransition>::Err( priority.Error() );
    out.m_Priority = priority.Value();

    return asge::Result<PendingTransition>::Ok( out );
}

}

int asge::game::asset::AnimationStateMachine::GetIndexFrom(
    std::vector<str::String> const &inVec, str::StringView inVal) noexcept
{
    for ( std::size_t ii = 0; ii < inVec.size(); ++ii )
    {
        if ( inVec[ii] == inVal ) return ii;
    }
    return -1;
}

asge::Result<asge::game::asset::AnimationStateMachine>
asge::game::asset::AnimationStateMachine::Load(filesystem::Path const &inPath)
{
    using Out = Result<AnimationStateMachine>;

    auto parseResult = formats::xml::Parse( inPath );
    if ( !parseResult ) return Out::Err( parseResult.Error() );

    auto const& rootNode = parseResult.Value();
    if ( rootNode.Name() != "AnimStateMachine" )
    {
        return Out::Err( make_error_code( errors::AnimStateMachineErrors::UnexpectedElement ),
            "root element must be <AnimStateMachine>, found <" + rootNode.Name() + ">" );
    }

    // The initial attribute is required and cannot be blank
    auto const* initialAttr = rootNode.Find( "initial" );
    str::StringView const initialState = initialAttr ? str::Trim( initialAttr->Value() ) : str::StringView{};
    if ( initialState.empty() )
    {
        return Out::Err( make_error_code( errors::AnimStateMachineErrors::MissingInitialState ) );
    }

    AnimationStateMachine sm;
    std::vector<formats::xml::XMLNode const*> stateNodes;

    // First pass: collect every flag, trigger, clip and state name, so that
    // states and transitions can refer to names declared further down the file
    for ( auto const& child : rootNode )
    {
        BoolResult result = BoolResult::Ok();

        if ( child.Name() == "Flags" )
        {
            result = ParseNameList( child, "Flag", sm.m_FlagNames, kMaxFlags );
        }
        else if ( child.Name() == "Triggers" )
        {
            result = ParseNameList( child, "Trigger", sm.m_TriggerNames, kMaxTriggers );
        }
        else if ( child.Name() == "Clips" )
        {
            result = ParseNameList( child, "Clip", sm.m_ClipNames, kMaxStates );
        }
        else if ( child.Name() == "State" )
        {
            auto name = RequiredAttr( child, "name", "<State>" );
            if ( !name ) return Out::Err( name.Error() );

            result = AddName( sm.m_StateNames, name.Value(), "state", kMaxStates );
            if ( result ) stateNodes.push_back( &child );
        }
        else
        {
            return Out::Err( make_error_code( errors::AnimStateMachineErrors::UnexpectedElement ),
                "unknown element <" + child.Name() + "> in <AnimStateMachine>" );
        }

        if ( !result ) return Out::Err( result.Error() );
    }

    auto const initialIdx = GetIndexFrom( sm.m_StateNames, initialState );
    if ( initialIdx < 0 )
    {
        return Out::Err( make_error_code( errors::AnimStateMachineErrors::StateNotFound ),
            "initial state '" + str::String( initialState ) + "' is not declared" );
    }
    sm.m_Initial = static_cast<std::uint16_t>( initialIdx );

    // Second pass: build the states and their transitions in CSR layout
    sm.m_States.reserve( stateNodes.size() );

    std::vector<PendingTransition> pending;
    for ( std::size_t ii = 0; ii < stateNodes.size(); ++ii )
    {
        auto const& node = *stateNodes[ii];
        str::String const& stateName = sm.m_StateNames[ii];
        str::String const ctx = "<State '" + stateName + "'>";

        State state;

        auto clip = RequiredAttr( node, "clip", ctx );
        if ( !clip ) return Out::Err( clip.Error() );

        auto const clipIdx = GetIndexFrom( sm.m_ClipNames, clip.Value() );
        if ( clipIdx < 0 )
        {
            return Out::Err( make_error_code( errors::AnimStateMachineErrors::ClipNotFound ),
                ctx + " uses clip '" + str::String( clip.Value() ) + "' which is not declared in <Clips>" );
        }
        state.m_ClipId = static_cast<std::uint16_t>( clipIdx );

        auto endMode = ParseEndMode( node, ctx );
        if ( !endMode ) return Out::Err( endMode.Error() );
        state.m_End = endMode.Value();

        pending.clear();
        for ( auto const& child : node )
        {
            if ( child.Name() != "Transition" )
            {
                return Out::Err( make_error_code( errors::AnimStateMachineErrors::UnexpectedElement ),
                    ctx + " can only contain <Transition>, found <" + child.Name() + ">" );
            }

            auto transition = ParseTransition( child, stateName,
                sm.m_StateNames, sm.m_FlagNames, sm.m_TriggerNames );
            if ( !transition ) return Out::Err( transition.Error() );
            pending.push_back( transition.Value() );
        }

        // Highest priority first; equal priorities keep their document order
        std::ranges::stable_sort( pending, std::greater<>{}, &PendingTransition::m_Priority );

        state.m_FirstTransition = static_cast<std::uint32_t>( sm.m_Transitions.size() );
        state.m_TransitionCount = static_cast<std::uint32_t>( pending.size() );
        for ( auto const& entry : pending ) sm.m_Transitions.push_back( entry.m_Transition );

        sm.m_States.push_back( state );
    }

    return Out::Ok( std::move( sm ) );
}

std::span<asge::game::asset::AnimationStateMachine::State const>
asge::game::asset::AnimationStateMachine::States() const noexcept
{
    return m_States;
}

std::span<asge::game::asset::AnimationStateMachine::Transition const>
asge::game::asset::AnimationStateMachine::TransitionsOf(std::uint16_t inState) const noexcept
{
    State const& s = m_States[inState];
    return { m_Transitions.data() + s.m_FirstTransition, s.m_TransitionCount };
}

std::uint16_t asge::game::asset::AnimationStateMachine::Initial() const noexcept
{
    return m_Initial;
}

std::span<asge::str::String const> asge::game::asset::AnimationStateMachine::ClipNames() const noexcept
{
    return m_ClipNames;
}

asge::Result<std::uint16_t> asge::game::asset::AnimationStateMachine::FindState(
    str::StringView inName) const noexcept
{
    auto idx = GetIndexFrom( m_StateNames, inName );
    if ( idx < 0 )
    {
        return Result<std::uint16_t>::Err(
            make_error_code(errors::AnimStateMachineErrors::StateNotFound),
            str::String(inName)
        );
    }

    return Result<std::uint16_t>::Ok( static_cast<std::uint16_t>(idx) );
}

asge::Result<std::uint8_t> asge::game::asset::AnimationStateMachine::FindFlag(
    str::StringView inName) const noexcept
{
    auto idx = GetIndexFrom( m_FlagNames, inName );
    if ( idx < 0 )
    {
        return Result<std::uint8_t>::Err(
            make_error_code(errors::AnimStateMachineErrors::FlagNotFound),
            str::String(inName)
        );
    }

    return Result<std::uint8_t>::Ok( static_cast<std::uint8_t>(idx) );
}

asge::Result<std::uint8_t> asge::game::asset::AnimationStateMachine::FindTrigger(
    str::StringView inName) const noexcept
{
    auto idx = GetIndexFrom( m_TriggerNames, inName );
    if ( idx < 0 )
    {
        return Result<std::uint8_t>::Err(
            make_error_code(errors::AnimStateMachineErrors::TriggerNotFound),
            str::String(inName)
        );
    }

    return Result<std::uint8_t>::Ok( static_cast<std::uint8_t>(idx) );
}

asge::Result<std::uint16_t> asge::game::asset::AnimationStateMachine::FindClip(
    str::StringView inName) const noexcept
{
    auto idx = GetIndexFrom( m_ClipNames, inName );
    if ( idx < 0 )
    {
        return Result<std::uint16_t>::Err(
            make_error_code(errors::AnimStateMachineErrors::ClipNotFound),
            str::String(inName)
        );
    }

    return Result<std::uint16_t>::Ok( static_cast<std::uint16_t>(idx) );
}
