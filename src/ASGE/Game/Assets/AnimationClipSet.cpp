#include "AnimationClipSet.hpp"

#include <algorithm>

#include "AnimXml.hpp"
#include "AssetManager.hpp"

using namespace asge::game::asset::_internal;

asge::Result<asge::game::asset::AnimationClipSet>
asge::game::asset::AnimationClipSet::Load(filesystem::Path const &inPath)
{
    using Out = Result<AnimationClipSet>;

    auto parseResult = formats::xml::Parse( inPath );
    if ( !parseResult ) return Out::Err( parseResult.Error() );

    auto const& rootNode = parseResult.Value();
    if ( rootNode.Name() != "AnimClipSet" )
    {
        return Out::Err( make_error_code( errors::AnimStateMachineErrors::UnexpectedElement ),
            "root element must be <AnimClipSet>, found <" + rootNode.Name() + ">" );
    }

    AnimationClipSet set;

    auto machine = RequiredAttr( rootNode, "machine", "<AnimClipSet>" );
    if ( !machine ) return Out::Err( machine.Error() );
    set.m_MachinePath = str::String( machine.Value() );

    for ( auto const& child : rootNode )
    {
        if ( child.Name() != "Clip" )
        {
            return Out::Err( make_error_code( errors::AnimStateMachineErrors::UnexpectedElement ),
                "<AnimClipSet> can only contain <Clip>, found <" + child.Name() + ">" );
        }

        auto name = RequiredAttr( child, "name", "<Clip>" );
        if ( !name ) return Out::Err( name.Error() );

        auto added = AddName( set.m_ClipNames, name.Value(), "clip",
            AnimationStateMachine::kMaxStates );
        if ( !added ) return Out::Err( added.Error() );

        str::String const ctx = "<Clip '" + str::String( name.Value() ) + "'>";

        auto path = RequiredAttr( child, "path", ctx );
        if ( !path ) return Out::Err( path.Error() );

        Binding binding;
        auto duration = ConvertedAttr<float>( child, "frameDuration", ctx, binding.m_FrameDuration,
            []( auto const& inAttr ) { return inAttr.AsFloat(); } );
        if ( !duration ) return Out::Err( duration.Error() );
        if ( duration.Value() <= 0.0f )
        {
            return Out::Err( make_error_code( errors::AnimStateMachineErrors::InvalidAttribute ),
                ctx + " attribute 'frameDuration' must be greater than zero" );
        }
        binding.m_FrameDuration = duration.Value();

        set.m_ClipPaths.emplace_back( path.Value() );
        set.m_Bindings.push_back( std::move( binding ) );
    }

    return Out::Ok( std::move( set ) );
}

asge::BoolResult asge::game::asset::AnimationClipSet::ResolveAssets(AssetManager &inAssetManager)
{
    auto machine = inAssetManager.GetStateMachine( m_MachinePath );
    if ( !machine ) return BoolResult::Err( machine.Error() );

    auto const machineClips = machine.Value()->Get().ClipNames();

    // Both sides must agree on the set of clips
    for ( auto const& name : m_ClipNames )
    {
        if ( std::ranges::find( machineClips, name ) == machineClips.end() )
        {
            return BoolResult::Err( make_error_code( errors::AnimStateMachineErrors::ClipNotFound ),
                "clip '" + name + "' is not declared by machine '" + m_MachinePath + "'" );
        }
    }

    // Build the reordered tables on the side, so a failure leaves *this untouched
    std::vector<Binding>     bindings;
    std::vector<str::String> paths;
    bindings.reserve( machineClips.size() );
    paths.reserve( machineClips.size() );

    for ( auto const& name : machineClips )
    {
        int const idx = IndexOf( m_ClipNames, name );
        if ( idx < 0 )
        {
            return BoolResult::Err( make_error_code( errors::AnimStateMachineErrors::ClipNotFound ),
                "machine clip '" + name + "' has no binding in the clip set" );
        }

        auto table = inAssetManager.GetFrameTable( m_ClipPaths[idx] );
        if ( !table ) return BoolResult::Err( table.Error() );

        Binding binding = m_Bindings[idx];
        binding.m_Clip = table.Value();
        bindings.push_back( std::move( binding ) );
        paths.push_back( m_ClipPaths[idx] );
    }

    m_Machine  = machine.Value();
    m_Bindings = std::move( bindings );
    m_ClipPaths = std::move( paths );
    m_ClipNames.assign( machineClips.begin(), machineClips.end() );
    return BoolResult::Ok();
}

bool asge::game::asset::AnimationClipSet::IsResolved() const noexcept
{
    return m_Machine != nullptr;
}

asge::game::asset::AnimationStateMachine const&
asge::game::asset::AnimationClipSet::Machine() const noexcept
{
    return m_Machine->Get();
}

std::span<asge::game::asset::AnimationClipSet::Binding const>
asge::game::asset::AnimationClipSet::Bindings() const noexcept
{
    return m_Bindings;
}

asge::str::StringView asge::game::asset::AnimationClipSet::MachinePath() const noexcept
{
    return m_MachinePath;
}

std::span<asge::str::String const> asge::game::asset::AnimationClipSet::ClipNames() const noexcept
{
    return m_ClipNames;
}

std::span<asge::str::String const> asge::game::asset::AnimationClipSet::ClipPaths() const noexcept
{
    return m_ClipPaths;
}
